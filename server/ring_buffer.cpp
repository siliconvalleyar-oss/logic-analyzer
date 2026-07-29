//==============================================================================
// ring_buffer.cpp
// Buffer circular lock-free SPSC
// Licencia: MIT
//==============================================================================

#include "ring_buffer.h"

RingBuffer::RingBuffer(size_t capacity)
    : capacity_(capacity), write_idx_(0) {
    buffer_.resize(capacity_);
}

void RingBuffer::push(uint64_t ts, uint32_t state) {
    size_t next = (write_idx_ + 1) % capacity_;
    if (next == read_idx_.load(std::memory_order_acquire)) {
        read_idx_.store((read_idx_.load(std::memory_order_relaxed) + 1) % capacity_,
                        std::memory_order_release);
    }
    buffer_[write_idx_] = {ts, state};
    write_idx_ = next;
}

std::vector<Sample> RingBuffer::drain() {
    std::vector<Sample> r;
    size_t rd = read_idx_.load(std::memory_order_acquire);
    size_t wr = write_idx_;
    if (rd == wr) return r;

    if (wr > rd) {
        r.reserve(wr - rd);
        r.insert(r.end(), buffer_.begin() + rd, buffer_.begin() + wr);
    } else {
        r.reserve(capacity_ - rd + wr);
        r.insert(r.end(), buffer_.begin() + rd, buffer_.end());
        r.insert(r.end(), buffer_.begin(), buffer_.begin() + wr);
    }
    read_idx_.store(wr, std::memory_order_release);
    return r;
}

size_t RingBuffer::size() const {
    size_t r = read_idx_.load(std::memory_order_acquire);
    size_t w = write_idx_;
    return (w >= r) ? w - r : capacity_ - r + w;
}
