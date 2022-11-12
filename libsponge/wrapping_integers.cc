#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) { return isn + static_cast<uint32_t>(n); }

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    // n 可以在 checkpoint 的左边，也可以在 checkpoint 的右边，
    // 留意无符号数相减求偏移量，例如：
    // n 为 1，checkpoint 为 7，1 - 7 等于 1<<32 + 1 - 7
    // 所以如果新位置距离 checkpoint 的偏移量大于 1<<32 的一半，即 1<<31
    // 那么离 checkpoint 最近的位置实际上是在 checkpoint 的左侧

    uint32_t offset = n - wrap(checkpoint, isn);
    uint64_t pos = checkpoint + offset;
    if (offset > (1u << 31) && pos >= (1ul << 32)) {
        pos -= (1ul << 32);
    }
    return pos;
}
