#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity), _capacity(capacity), _unassembled_bytes(0), _is_eof(false), _eof_idx(0), _buffer() {}

void StreamReassembler::_buffer_erase(const set<Segment>::iterator &iter) {
    _unassembled_bytes -= iter->length();
    _buffer.erase(iter);
}

void StreamReassembler::_buffer_insert(const Segment &seg) {
    _unassembled_bytes += seg.length();
    _buffer.insert(seg);
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    // process the input segment
    if (!data.empty()) {  // data != ""
        Segment seg{index, data};
        _handle_substring(seg);
    }

    // write to 'ByteStream'
    while (!_buffer.empty() && _buffer.begin()->_idx == _1st_unassembled_idx()) {
        const auto &iter = _buffer.begin();
        _output.write(iter->_data);
        _buffer_erase(iter);
    }

    // EOF
    if (eof) {
        _is_eof = eof;
        _eof_idx = index + data.length();
    }
    if (_is_eof && _1st_unassembled_idx() == _eof_idx) {
        _output.end_input();
    }
}

void StreamReassembler::_handle_substring(Segment &seg) {
    // 检查边界

    /**
     * @brief seg 首字节 index 超出缓冲区右侧边界，丢弃 seg。
     *
     *           index
     *         │  ├─────────────┐
     *   ──────┼──┴─────────────┴──►
     *       first
     *     unacceptable
     */
    if (seg._idx >= _1st_unacceptabled_idx()) {
        return;
    }

    /**
     * @brief seg 尾部超出缓冲区右侧边界，裁切 seg 尾部。
     *
     *    index
     *      ├────────┼────┐
     *   ───┴────────┼────┴────►
     *             first
     *           unacceptable
     */
    if (seg._idx < _1st_unacceptabled_idx() && seg._idx + seg.length() - 1 >= _1st_unacceptabled_idx()) {
        seg._data = seg._data.substr(0, _1st_unacceptabled_idx() - seg._idx);
    }

    /**
     * @brief seg 尾部小于缓冲区左侧边界，即 seg 已经全部被写入过 ByteStream，丢弃 seg。
     *
     *   index
     *     ├───────────┐  │
     *   ──┴───────────┴──┼─────────►
     *                  first
     *               unassembled
     */
    if (seg._idx + seg.length() - 1 < _1st_unassembled_idx()) {
        return;
    }

    /**
     * @brief seg 头部小于缓冲区左侧边界，即 seg 被部分写入过 ByteStream，裁切 seg 头部。
     *
     *    index
     *      ├──────┼────┐
     *  ────┴──────┼────┴────►
     *           first
     *        unassembled
     */
    if (seg._idx < _1st_unassembled_idx() && seg._idx + seg.length() - 1 >= _1st_unassembled_idx()) {
        seg._data = seg._data.substr(_1st_unassembled_idx() - seg._idx);
        seg._idx = _1st_unassembled_idx();
    }

    // -----------------------------------------------
    // seg 可以放入缓冲区中，并与已经存在的 unassembled segments 进行比较。

    if (_buffer.empty()) {
        _buffer_insert(seg);
        return;
    }

    // 处理重叠部分
    _handle_overlap(seg);
}

void StreamReassembler::_handle_overlap(Segment &seg) {
    // 从头遍历 O(n)，可以考虑使用 upper_bound() 二分查找 O(logn) 优化一下
    for (auto iter = _buffer.begin(); iter != _buffer.end();) {
        size_t seg_tail = seg._idx + seg.length() - 1;
        size_t cache_tail = iter->_idx + iter->length() - 1;

        if ((seg._idx >= iter->_idx && seg._idx <= cache_tail) || (iter->_idx >= seg._idx && iter->_idx <= seg_tail)) {
            _merge_seg(seg, *iter);  // 先消除重叠：把传入的 seg 与已缓存并存在重叠的 segment 合并,
            _buffer_erase(iter++);   // 合并后先删除已存在的 segment
        } else {
            ++iter;
        }
    }

    /**
     * @brief data 与已经缓存的 segments 之间没有重叠，可以存入缓冲区
     *
     *             index     tail
     *     ┌─────┐  ├─────────┤   ┌────────┐
     *   ──┴─────┴──┴─────────┴───┴────────┴────►
     */
    _buffer_insert(seg);  // 把处理完重叠并合并后的 seg 插入缓冲区
}

void StreamReassembler::_merge_seg(Segment &seg, const Segment &cache) {
    size_t seg_tail = seg._idx + seg.length() - 1;
    size_t cache_tail = cache._idx + cache.length() - 1;

    /**
     * @brief seg 尾部与已缓存的 segment 重合，裁切 seg 尾部再合并
     *
     *   seg           seg
     *   index         tail
     *     ├────────────┤             ┌──────────┐
     *     │////////////│             │//////////│
     *     └──────┬─────┴───┐         └─┬────────┤
     *            │         │           │        │
     *        ────┼─────────┼───────────┴────────┴───►
     *        stored   1   stored           2
     *    segment index   segment tail
     */
    if (seg._idx < cache._idx && seg_tail <= cache_tail) {
        seg._data = seg._data.substr(0, cache._idx - seg._idx) + cache._data;
    }
    /**
     * @brief seg 头部与已缓存的 segment 重合，裁切 seg 头部再合并
     *
     *               seg        seg
     *               index      tail
     *                 ├──────────┤      ┌─────────────┐
     *                 │//////////│      │/////////////│
     *             ┌───┴─────┬────┘      ├─────────┬───┘
     *             │         │           │         │
     *         ────┼─────────┼───────────┴─────────┴─────────────►
     *         stored   1    stored           2
     *     segment index   segment tail
     */
    else if (seg._idx >= cache._idx && seg_tail > cache_tail) {
        seg._data = cache._data + seg._data.substr(cache._idx + cache.length() - seg._idx);
        seg._idx = cache._idx;
    }
    /**
     * @brief seg 所包含的字节内容已经存在于缓冲区中。
     *
     *         seg       seg
     *         index     tail
     *           ├────────┤         ┌─────┐           ┌────┐  ┌───────┐
     *           │////////│         │/////│           │////│  │///////│
     *       ┌───┴────────┴───┐     ├─────┴────┐  ┌───┴────┤  ├───────┤
     *       │                │     │          │  │        │  │       │
     *     ──┼────────────────┼─────┴──────────┴──┴────────┴──┴───────┴───►
     *    stored      1     stored       2            3          4
     *   segment index    segment tail
     */
    else if (seg._idx >= cache._idx && seg_tail <= cache_tail) {
        seg._data = cache._data;
        seg._idx = cache._idx;
    }
    /**
     * @brief seg 中部与已经缓存的 segment 重合
     *
     *       seg               seg
     *       index             tail
     *         ├─────────────────┤   ┌─────────────────┐
     *         │/////////////////│   │/////////////////│
     *         └───┬─────────┬───┘   └─┬───┬───┬───┬───┘
     *             │         │         │   │   │   │
     *         ────┼─────────┼─────────┴───┴───┴───┴───►
     *         stored   1   stored           2
     *     segment index   segment tail
     */
    // else if (seg._idx < cache._idx && seg_tail > cache_tail) {
    // do nothing
    // }
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes; }

bool StreamReassembler::empty() const { return _buffer.empty(); }
