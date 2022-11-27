#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { return _next_seqno - _last_ackno; }

void TCPSender::fill_window() {
    TCPSegment seg;

    // CLOSED: waiting for stream to begin(no SYN sent)
    if (next_seqno_absolute() == 0) {
        seg.header().syn = true;
        send_segment(seg);
        return;
    }
    // SYN_SENT: stream started but nothing acknowledged
    else if (next_seqno_absolute() > 0 && next_seqno_absolute() == bytes_in_flight()) {
        return;
    }

    // 如果 window size 为 0，发送方按照接收窗口为 1 的情况发包
    uint16_t current_window_size = _last_window_size == 0 ? 1 : _last_window_size;
    size_t remaining_window_size = 0;
    while ((remaining_window_size = current_window_size - bytes_in_flight())) {
        // SYN_ACKED: stream ongoing
        if (!_stream.eof() && next_seqno_absolute() > bytes_in_flight()) {
            size_t payload_size = min(TCPConfig::MAX_PAYLOAD_SIZE, remaining_window_size);
            seg.payload() = Buffer(_stream.read(payload_size));
            if (_stream.eof() && seg.length_in_sequence_space() < remaining_window_size) {  // 能放下 FIN flag
                seg.header().fin = true;
            }
            if (seg.length_in_sequence_space() == 0) {  // 没有数据则不发送
                return;
            }
            send_segment(seg);
        } else if (_stream.eof()) {
            // SYN_ACKED: stream ongoing(stream has reached EOF, but FIN flag hasn't been sent yet)
            if (next_seqno_absolute() < _stream.bytes_written() + 2) {
                seg.header().fin = true;
                send_segment(seg);
                return;
            }
            // FIN_SENT: stream finished (FIN sent) but not fully acknowledged
            // FIN_ACKED: stream finished and fully acknowledged
            else {
                return;
            }
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t abs_ackno = unwrap(ackno, _isn, _last_ackno);
    // 丢弃不可靠的 ack
    if (abs_ackno > _next_seqno) {
        return;
    }
    // 如果收到的 ackno 大于任何之前的 ackno
    if (abs_ackno > _last_ackno) {
        _last_ackno = abs_ackno;

        // 丢弃已经被确认的 outstanding segments
        while (!_segments_outstanding.empty()) {
            const TCPSegment &seg = _segments_outstanding.front();
            if (seg.header().seqno.raw_value() + seg.length_in_sequence_space() <= ackno.raw_value()) {
                _segments_outstanding.pop();
            } else {
                break;
            }
        }

        _RTO = _initial_retransmission_timeout;  // 重置 RTO 为初始值
        _consecutive_retransmission_counts = 0;  // 重置连续重传计数为 0
        // outstanding segments 非空，重启定时器
        if (!_segments_outstanding.empty()) {
            _timer.start(_RTO);
        }
        // 否则停止定时器
        else {
            _timer.stop();
        }
    }
    _last_window_size = window_size;
    fill_window();  // 如果 window size 有空余空间，继续发包
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call
//! to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _timer.tick(ms_since_last_tick);

    // outstanding segments 不为空，同时定时器正在运行且已经过期
    if (!_segments_outstanding.empty() && _timer.is_expired()) {
        // 重传最早未被确认的片段
        _segments_out.push(_segments_outstanding.front());
        // window size 非空
        if (_last_window_size > 0) {
            _consecutive_retransmission_counts++;  // 增加连续重传计数
            _RTO *= 2;                             // RTO 翻倍
        }
        // 重置并重启定时器
        _timer.start(_RTO);
    }
    // outstanding segments 为空，停止计时器
    else if (_segments_outstanding.empty()) {
        _timer.stop();
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmission_counts; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = next_seqno();
    _segments_out.push(seg);
}

void TCPSender::send_segment(TCPSegment &seg) {
    seg.header().seqno = next_seqno();
    _next_seqno += seg.length_in_sequence_space();
    _segments_out.push(seg);
    _segments_outstanding.push(seg);

    // 如果定时器没有运行，启动定时器
    if (!_timer.is_started()) {
        _timer.start(_RTO);
    }
}
