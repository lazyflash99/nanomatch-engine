#pragma once

#include <cstdint>
#if defined(__x86_64__) || defined(__amd64__)
#include <x86intrin.h>
#endif

namespace nanomatch {

/**
 * @brief High-resolution timer using CPU cycle counter (RDTSC).
 * Provides the most accurate measurement of "Tick-to-Trade" latency.
 */
inline uint64_t rdtsc() {
#if defined(__x86_64__) || defined(__amd64__)
    return __rdtsc();
#else
    return 0; // Fallback for non-x86
#endif
}

} // namespace nanomatch
