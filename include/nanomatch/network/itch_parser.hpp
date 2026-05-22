#pragma once

#include "nanomatch/core/types.hpp"
#include <string_view>

namespace nanomatch {

/**
 * @brief Zero-copy ITCH-like Binary Parser.
 * Assumes a fixed-width binary format for maximum speed.
 */
class ITCHParser {
public:
    // Example Binary Message Format (Simple Add Order):
    // [Type:1][OrderID:8][Side:1][Qty:4][Price:8][InstrumentID:4] = 26 bytes
    
    struct AddOrderMsg {
        uint64_t order_id;
        char side;
        uint32_t quantity;
        int64_t price;
        uint32_t instrument_id;
    } __attribute__((packed));

    /**
     * @brief Parse an "Add Order" message from a raw pointer.
     * Uses zero-copy by casting the pointer directly to the packed struct.
     */
    static const AddOrderMsg* parse_add_order(const char* buffer) {
        return reinterpret_cast<const AddOrderMsg*>(buffer);
    }

    /**
     * @brief Utility to convert binary Side to our Side enum.
     */
    static Side convert_side(char side_char) {
        return (side_char == 'B' || side_char == '0') ? Side::BUY : Side::SELL;
    }
};

} // namespace nanomatch
