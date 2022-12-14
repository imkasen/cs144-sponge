#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    if (!_is_active) {
        return;
    }
    _time_since_last_segment_received = 0;

    // 接收到 RST 标识
    if (seg.header().rst) {
        unclean_shutdown();
        return;
    }

    // 交给接收器
    _receiver.segment_received(seg);

    // 服务端在 LISTEN 状态接收到了 SYN
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::SYN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::CLOSED) {
        // 进行三次握手中的第二次，发送 SYN + ACK
        connect();
        return;
    }

    // 接收到 ACK 标识，通知发送器更新
    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
    }
    // _sender.fill_window(); // ack_received() 中已经调用

    // 至少发送一个 segment 作为回复
    if (seg.length_in_sequence_space() > 0 && _sender.segments_out().empty()) {
        _sender.send_empty_segment();
    }

    // keep-alive
    if (_receiver.ackno().has_value() && seg.length_in_sequence_space() == 0 &&
        seg.header().seqno == _receiver.ackno().value() - 1) {
        _sender.send_empty_segment();
    }

    send_segments();
    // 尝试 clean shutdown
    clean_shutdown();
}

bool TCPConnection::active() const { return _is_active; }

size_t TCPConnection::write(const string &data) {
    if (!_is_active || data.empty()) {
        return 0;
    }
    size_t n = _sender.stream_in().write(data);
    _sender.fill_window();
    send_segments();
    return n;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    if (!_is_active) {
        return;
    }
    // 告知时间
    _sender.tick(ms_since_last_tick);
    _time_since_last_segment_received += ms_since_last_tick;
    // 超出限制则终止连接
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        send_rst_segment();  // 空或者非空 segment 似乎都行
        unclean_shutdown();
        return;
    }
    // 发送 _sender.tick() 中的重传片段
    send_segments();
    // 尝试 clean shutdown
    clean_shutdown();
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    // 结束流，发送 FIN 报文
    _sender.fill_window();
    send_segments();
}

void TCPConnection::connect() {  // 用于三次握手
    _sender.fill_window();
    send_segments();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            send_rst_segment();
            unclean_shutdown();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::send_segments() {
    while (!_sender.segments_out().empty()) {
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        if (_receiver.ackno().has_value()) {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
            seg.header().win = _receiver.window_size() <= numeric_limits<uint16_t>::max()
                                   ? _receiver.window_size()
                                   : numeric_limits<uint16_t>::max();
        }
        _segments_out.push(seg);
    }
}

void TCPConnection::send_rst_segment() {
    _sender.fill_window();
    if (_sender.segments_out().empty()) {
        _sender.send_empty_segment();
    }

    TCPSegment seg = _sender.segments_out().front();
    _sender.segments_out().pop();
    if (_receiver.ackno().has_value()) {
        seg.header().ack = true;
        seg.header().ackno = _receiver.ackno().value();
        seg.header().win = _receiver.window_size() <= numeric_limits<uint16_t>::max() ? _receiver.window_size()
                                                                                      : numeric_limits<uint16_t>::max();
    }
    seg.header().rst = true;
    _segments_out.push(seg);
}

void TCPConnection::clean_shutdown() {
    // 输入流在输出流到达 EOF 之前结束（服务端）
    if (_receiver.stream_out().input_ended() && !_sender.stream_in().eof()) {
        _linger_after_streams_finish = false;
    }
    // 客户端处在 TIME-WAIT 时
    else if (TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED &&
             TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV) {
        if (!_linger_after_streams_finish || _time_since_last_segment_received >= 10 * _cfg.rt_timeout) {
            _is_active = false;
        }
    }
}

void TCPConnection::unclean_shutdown() {
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _is_active = false;
}
