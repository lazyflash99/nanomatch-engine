#pragma once

#include <atomic>
#include <array>
#include <optional>

namespace nanomatch {

/**
 * @brief Lock-free Single-Producer Single-Consumer (SPSC) Ring Buffer.
 */
template <typename T, size_t Capacity>
class SPSCQueue {
public:
    static_assert((Capacity & (Capacity - 1)) == 0);

    SPSCQueue() : head_(0), tail_(0) {}

    bool push(const T& item) {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        const size_t next_tail = (current_tail + 1) & mask_;

        if (next_tail == head_.load(std::memory_order_acquire)) return false;

        buffer_[current_tail] = item;
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    std::optional<T> pop() {
        const size_t current_head = head_.load(std::memory_order_relaxed);
        if (current_head == tail_.load(std::memory_order_acquire)) return std::nullopt;

        T item = buffer_[current_head];
        head_.store((current_head + 1) & mask_, std::memory_order_release);
        return item;
    }

    bool available_for_pop() const {
        return head_.load(std::memory_order_relaxed) != tail_.load(std::memory_order_acquire);
    }

private:
    alignas(64) std::array<T, Capacity> buffer_;
    static constexpr size_t mask_ = Capacity - 1;

    alignas(64) std::atomic<size_t> head_;
    alignas(64) std::atomic<size_t> tail_;
};

} // namespace nanomatch
