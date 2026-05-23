#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
#include <concepts>

namespace nanomatch {

template <typename T, size_t Capacity>
class ObjectPool {
public:
    static_assert(Capacity > 0);

    ObjectPool() {
        for (size_t i = 0; i < Capacity; ++i) free_indices_[i] = i;
        free_top_ = Capacity;
    }

    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

    T* acquire() {
        if (free_top_ == 0) [[unlikely]] return nullptr;
        return &pool_[free_indices_[--free_top_]];
    }

    void release(T* ptr) {
        size_t index = static_cast<size_t>(ptr - pool_.data());
        if (index < Capacity) [[likely]] free_indices_[free_top_++] = index;
    }

    size_t available() const { return free_top_; }

private:
    alignas(64) std::array<T, Capacity> pool_;
    std::array<size_t, Capacity> free_indices_;
    size_t free_top_;
};

} // namespace nanomatch
