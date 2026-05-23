#pragma once

#include "nanomatch/core/types.hpp"
#include <string_view>
#if defined(__linux__)
#include <endian.h>
#elif defined(__APPLE__)
#include <libkern/OSByteOrder.h>
#define be64toh(x) OSSwapBigToHostInt64(x)
#define be32toh(x) OSSwapBigToHostInt32(x)
#endif

namespace nanomatch {

/**
 * @brief Zero-copy ITCH-like Binary Parser with Endianness correction.
 */
class ITCHParser {
public:
    struct AddOrderMsg {
        uint64_t order_id;
        char side;
        uint32_t quantity;
        int64_t price;
        uint32_t instrument_id;
    } __attribute__((packed));

    /**
     * @brief Parse and convert from Network Order (Big Endian) to Host Order.
     */
    static void parse_and_fill(const char* buffer, Order& order) {
        const auto* msg = reinterpret_cast<const AddOrderMsg*>(buffer);
        
        order.order_id = be64toh(msg->order_id);
        order.side = (msg->side == 'B') ? Side::BUY : Side::SELL;
        order.quantity = be32toh(msg->quantity);
        order.price = static_cast<int64_t>(be64toh(static_cast<uint64_t>(msg->price)));
        order.instrument_id = be32toh(msg->instrument_id);
    }

    static uint64_t get_order_id(const char* buffer) {
        const auto* msg = reinterpret_cast<const AddOrderMsg*>(buffer);
        return be64toh(msg->order_id);
    }

    static Side convert_side(char side_char) {
        return (side_char == 'B' || side_char == '0') ? Side::BUY : Side::SELL;
    }
};

} // namespace nanomatch
