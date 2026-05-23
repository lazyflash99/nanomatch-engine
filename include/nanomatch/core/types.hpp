#pragma once

#include <cstdint>

namespace nanomatch {

enum class Side : uint8_t { BUY = 0, SELL = 1 };
enum class OrderType : uint8_t { MARKET = 0, LIMIT = 1 };

struct PriceLevel;

struct alignas(64) Order {
    uint64_t order_id;
    int64_t  price;
    uint32_t quantity;
    uint32_t instrument_id;
    Side     side;
    OrderType type;

    Order* next = nullptr;
    Order* prev = nullptr;
    PriceLevel* level = nullptr;
    
    uint8_t padding[14];
};

struct alignas(64) TradeReport {
    uint64_t trade_id;
    uint64_t maker_id;
    uint64_t taker_id;
    int64_t  price;
    uint32_t quantity;
    uint32_t instrument_id;
};

} // namespace nanomatch
