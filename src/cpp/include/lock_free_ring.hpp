#ifndef LOCK_FREE_RING_HPP
#define LOCK_FREE_RING_HPP

#include <atomic>
#include <cstddef>
#include <memory>
#include <stdexcept>

#include "execution_affinity.hpp"

namespace order_flow {
namespace utils {

template <typename T>
class LockFreeRing {
public:
    explicit LockFreeRing(std::size_t capacity_pow2, const PinningConfig* cfg = nullptr)
        : capacity_(round_up(capacity_pow2)), mask_(capacity_ - 1), buffer_(nullptr), head_(0), tail_(0) {
        if (capacity_ < 2) {
            throw std::invalid_argument("Ring capacity too small");
        }
        std::size_t bytes = capacity_ * sizeof(T);
        if (cfg && cfg->memory_node >= 0) {
            numa_buffer_ = std::make_unique<NumaBuffer>(bytes, cfg->memory_alignment, *cfg);
            buffer_ = static_cast<T*>(numa_buffer_->data());
        } else {
            owned_buffer_.reset(new T[capacity_]);
            buffer_ = owned_buffer_.get();
            if (cfg && cfg->prefault_memory) {
                prefault_memory(buffer_, bytes);
            }
        }
    }

    bool try_push(const T& item) {
        std::size_t tail = tail_.load(std::memory_order_relaxed);
        std::size_t head = head_.load(std::memory_order_acquire);
        if (((tail + 1) & mask_) == (head & mask_)) {
            return false; // full
        }
        buffer_[tail & mask_] = item;
        tail_.store(tail + 1, std::memory_order_release);
        return true;
    }

    bool try_pop(T& item) {
        std::size_t head = head_.load(std::memory_order_relaxed);
        std::size_t tail = tail_.load(std::memory_order_acquire);
        if ((head & mask_) == (tail & mask_)) {
            return false; // empty
        }
        item = buffer_[head & mask_];
        head_.store(head + 1, std::memory_order_release);
        return true;
    }

    std::size_t capacity() const { return capacity_; }

private:
    static std::size_t round_up(std::size_t value) {
        std::size_t v = 1;
        while (v < value) {
            v <<= 1;
        }
        return v;
    }

    std::size_t capacity_;
    std::size_t mask_;
    std::unique_ptr<T[]> owned_buffer_;
    std::unique_ptr<NumaBuffer> numa_buffer_;
    T* buffer_;
    std::atomic<std::size_t> head_;
    std::atomic<std::size_t> tail_;
};

} // namespace utils
} // namespace order_flow

#endif // LOCK_FREE_RING_HPP
