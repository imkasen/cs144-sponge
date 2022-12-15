#include "byte_stream.hh"

#include <algorithm>
#include <string>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

using namespace std;

ByteStream::ByteStream(const size_t capacity)
    : _buffer(), _capacity(capacity), _written_size(0), _read_size(0), _is_end_input(false) {}

size_t ByteStream::write(const string &data) {
    if (input_ended()) {
        return 0;
    }

    size_t write_size = std::min(data.length(), remaining_capacity());
    _written_size += write_size;

    // 如果 write_size < data.length()，多余的部分会被丢弃
    /* lab0
    for (size_t i = 0; i < write_size; ++i) {
        _buffer.push_back(data[i]);
    }
    */
    // optimization in lab4
    string tmp = data.substr(0, write_size);
    _buffer.append(BufferList(std::move(tmp)));

    return write_size;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t peek_size = std::min(len, buffer_size());
    /* lab0
    return std::string(_buffer.begin(), _buffer.begin() + peek_size);
    */
    // optimization in lab4
    string s = _buffer.concatenate();
    return string(s.begin(), s.begin() + peek_size);
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t pop_size = std::min(len, buffer_size());
    _read_size += pop_size;
    /* lab0
    while (pop_size--) {
        _buffer.pop_front();
    }
    */
    // optimization in lab4
    _buffer.remove_prefix(pop_size);
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    std::string data = peek_output(len);
    pop_output(len);
    return data;
}

void ByteStream::end_input() { _is_end_input = true; }

bool ByteStream::input_ended() const { return _is_end_input; }

size_t ByteStream::buffer_size() const { return _buffer.size(); }

// bool ByteStream::buffer_empty() const { return _buffer.empty(); } // lab0
bool ByteStream::buffer_empty() const { return _buffer.size() == 0; }  // optimization in lab4

// eof 成立的条件是 writer 不再写入，同时 reader 读取完全部数据
bool ByteStream::eof() const { return input_ended() && buffer_empty(); }

size_t ByteStream::bytes_written() const { return _written_size; }

size_t ByteStream::bytes_read() const { return _read_size; }

size_t ByteStream::remaining_capacity() const { return _capacity - buffer_size(); }
