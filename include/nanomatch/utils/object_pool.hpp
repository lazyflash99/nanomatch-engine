#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
#include <concepts>

namespace nanomatch {

/**
 * @brief Memory-efficient, cache-aligned Object Pool.
 * Pre-allocates memory at startup to avoid OS calls in the hot path.
 */
template <typename T, size_t Capacity>
class ObjectPool {
public:
    static_assert(Capacity > 0, "Capacity must be greater than 0");

    ObjectPool() {
        for (size_t i = 0; i < Capacity; ++i) {
            free_indices_[i] = i;
        }
        free_top_ = Capacity;
    }

    // No copying or moving
    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

    /**
     * @brief Acquire a new object from the pool.
     * @return Pointer to the object, or nullptr if pool is exhausted.
     */
    T* acquire() {
        if (free_top_ == 0) [[unlikely]] {
            return nullptr;
        }
        return &pool_[free_indices_[--free_top_]];
    }

    /**
     * @brief Release an object back to the pool.
     */
    void release(T* ptr) {
        size_t index = static_cast<size_t>(ptr - pool_.data());
        if (index < Capacity) [[likely]] {
            free_indices_[free_top_++] = index;
        }
    }

    size_t available() const { return free_top_; }

private:
    alignas(64) std::array<T, Capacity> pool_;
    std::array<size_t, Capacity> free_indices_;
    size_t free_top_;
};

} // namespace nanomatch
