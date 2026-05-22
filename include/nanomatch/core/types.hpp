#pragma once

#include <cstdint>

namespace nanomatch {

enum class Side : uint8_t {
    BUY = 0,
    SELL = 1
};

enum class OrderType : uint8_t {
    MARKET = 0,
    LIMIT = 1
};

/**
 * @brief Packed Order structure.
 * Total size: 32 bytes (fits exactly in half a cache line).
 * Aligned to 64 bytes to prevent false sharing if managed in arrays.
 */
struct alignas(64) Order {
    uint64_t order_id;      // 8 bytes
    int64_t  price;         // 8 bytes
    uint32_t quantity;      // 4 bytes
    uint32_t instrument_id; // 4 bytes
    Side     side;          // 1 byte
    OrderType type;         // 1 byte
    
    // Linked list pointers for time priority within a price level
    Order* next = nullptr;  // 8 bytes
    Order* prev = nullptr;  // 8 bytes
    
    uint8_t padding[22];    // Explicit padding to 64 bytes
};

/**
 * @brief Trade Report for execution notifications.
 * Total size: 32 bytes.
 */
struct alignas(64) TradeReport {
    uint64_t trade_id;      // 8 bytes
    uint64_t maker_id;      // 8 bytes
    uint64_t taker_id;      // 8 bytes
    int64_t  price;         // 8 bytes
    uint32_t quantity;      // 4 bytes
    uint32_t instrument_id; // 4 bytes
};

} // namespace nanomatch
