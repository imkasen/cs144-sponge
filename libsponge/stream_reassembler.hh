#ifndef SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
#define SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH

#include "byte_stream.hh"

#include <cstdint>
#include <set>
#include <string>

//! \brief A class that assembles a series of excerpts from a byte stream (possibly out of order,
//! possibly overlapping) into an in-order byte stream.
class StreamReassembler {
  private:
    struct Segment {
        size_t _idx;
        std::string _data;

        Segment() : _idx(0), _data() {}
        Segment(size_t index, const std::string &data) : _idx(index), _data(data) {}

        size_t length() const { return _data.length(); }

        bool operator<(const Segment &seg) const { return this->_idx < seg._idx; }
    };

  private:
    // Your code here -- add private members as necessary.

    ByteStream _output;  //!< The reassembled in-order byte stream
    size_t _capacity;    //!< The maximum number of bytes

    size_t _unassembled_bytes;  // unassembled but stored bytes
    bool _is_eof;
    size_t _eof_idx;

    std::set<Segment> _buffer;
    void _buffer_erase(const std::set<Segment>::iterator &iter);
    void _buffer_insert(const Segment &seg);

    // 处理乱序、重叠的字符串片段，并尝试放入缓冲区中
    void _handle_substring(Segment &seg);
    void _handle_overlap(Segment &seg);
    void _merge_seg(Segment &seg, const Segment &cache);

    size_t _1st_unread_idx() const { return _output.bytes_read(); }
    size_t _1st_unassembled_idx() const { return _output.bytes_written(); }
    size_t _1st_unacceptabled_idx() const { return _1st_unread_idx() + _capacity; }

  public:
    //! \brief Construct a `StreamReassembler` that will store up to `capacity` bytes.
    //! \note This capacity limits both the bytes that have been reassembled,
    //! and those that have not yet been reassembled.
    StreamReassembler(const size_t capacity);

    //! \brief Receive a substring and write any newly contiguous bytes into the stream.
    //!
    //! The StreamReassembler will stay within the memory limits of the `capacity`.
    //! Bytes that would exceed the capacity are silently discarded.
    //!
    //! \param data the substring
    //! \param index indicates the index (place in sequence) of the first byte in `data`
    //! \param eof the last byte of `data` will be the last byte in the entire stream
    void push_substring(const std::string &data, const uint64_t index, const bool eof);

    //! \name Access the reassembled byte stream
    //!@{
    const ByteStream &stream_out() const { return _output; }
    ByteStream &stream_out() { return _output; }
    //!@}

    //! The number of bytes in the substrings stored but not yet reassembled
    //!
    //! \note If the byte at a particular index has been pushed more than once, it
    //! should only be counted once for the purpose of this function.
    size_t unassembled_bytes() const;

    //! \brief Is the internal state empty (other than the output stream)?
    //! \returns `true` if no substrings are waiting to be assembled
    bool empty() const;
};

#endif  // SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
