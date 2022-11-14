#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    const TCPHeader &header = seg.header();

    if (!_syn_flag) {
        if (!header.syn) {
            return;  // 直接丢弃
        }
        _syn_flag = header.syn;
        _isn = header.seqno;
    }
    // SYN_RECV 状态
    uint64_t checkpoint = header.syn ? 0 : stream_out().bytes_written() - 1;  // the index of last reassembled byte
    uint64_t abs_seqno = unwrap(header.seqno, _isn, checkpoint);
    uint64_t stream_idx = header.syn ? 0 : abs_seqno - 1;  // abs seqno 换算到 stream index，留意 SYN 为 true 的情况
    _reassembler.push_substring(seg.payload().copy(), stream_idx, header.fin);
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    // 非 LISTEN 状态
    if (!_syn_flag) {
        return std::nullopt;
    }
    // LISTEN 状态
    uint64_t abs_ackno = stream_out().bytes_written() + 1;  // 加上 SYN flag 的长度
    // FIN_RECV 状态
    if (stream_out().input_ended()) {
        abs_ackno += 1;  // 加上 FIN flag 的长度
    }
    return wrap(abs_ackno, _isn);
}

size_t TCPReceiver::window_size() const { return _capacity - stream_out().buffer_size(); }
