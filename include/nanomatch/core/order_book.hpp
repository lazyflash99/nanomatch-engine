#pragma once

#include "nanomatch/core/types.hpp"
#include "nanomatch/utils/object_pool.hpp"
#include "nanomatch/utils/spsc_queue.hpp"
#include <algorithm>
#include <array>
#include <functional>

namespace nanomatch {

struct PriceLevel {
    int64_t price = 0;
    uint32_t total_quantity = 0;
    Order* head = nullptr;
    Order* tail = nullptr;

    void add_order(Order* order) {
        if (!head) {
            head = tail = order;
            order->next = order->prev = nullptr;
        } else {
            tail->next = order;
            order->prev = tail;
            order->next = nullptr;
            tail = order;
        }
        total_quantity += order->quantity;
    }

    void remove_order(Order* order) {
        if (order->prev) order->prev->next = order->next;
        if (order->next) order->next->prev = order->prev;
        if (order == head) head = order->next;
        if (order == tail) tail = order->prev;
        
        total_quantity -= order->quantity;
        order->next = order->prev = nullptr;
    }
};

template <size_t MaxLevels = 1024, size_t MaxOrders = 10000>
class OrderBook {
public:
    OrderBook(uint32_t instrument_id, SPSCQueue<TradeReport, 1024>* report_queue = nullptr) 
        : instrument_id_(instrument_id), report_queue_(report_queue) {}

    /**
     * @brief Add a new order and attempt matching.
     */
    void add_order(Order* order) {
        if (order->side == Side::BUY) {
            match(order, ask_levels_, &num_asks_, std::less_equal<int64_t>{});
            if (order->quantity > 0 && order->type == OrderType::LIMIT) {
                add_to_book(order, bid_levels_, &num_bids_, std::greater<int64_t>{});
                order_map_[order->order_id % MaxOrders] = order;
            }
        } else {
            match(order, bid_levels_, &num_bids_, std::greater_equal<int64_t>{});
            if (order->quantity > 0 && order->type == OrderType::LIMIT) {
                add_to_book(order, ask_levels_, &num_asks_, std::less<int64_t>{});
                order_map_[order->order_id % MaxOrders] = order;
            }
        }
    }

    Order* cancel_order(uint64_t order_id) {
        Order* order = order_map_[order_id % MaxOrders];
        if (!order || order->order_id != order_id) return nullptr;

        auto& levels = (order->side == Side::BUY) ? bid_levels_ : ask_levels_;
        auto& count = (order->side == Side::BUY) ? num_bids_ : num_asks_;

        for (size_t i = 0; i < count; ++i) {
            if (levels[i].price == order->price) {
                levels[i].remove_order(order);
                if (levels[i].total_quantity == 0) {
                    std::move(levels.begin() + i + 1, levels.begin() + count, levels.begin() + i);
                    count--;
                }
                break;
            }
        }
        order_map_[order_id % MaxOrders] = nullptr;
        return order;
    }

private:
    template <typename Comparator>
    void match(Order* taker_order, std::array<PriceLevel, MaxLevels>& levels, size_t* num_levels, Comparator can_match) {
        size_t level_idx = 0;
        while (taker_order->quantity > 0 && level_idx < *num_levels) {
            auto& level = levels[level_idx];
            
            if (taker_order->type == OrderType::LIMIT && !can_match(taker_order->price, level.price)) {
                break;
            }

            Order* maker_order = level.head;
            while (maker_order && taker_order->quantity > 0) {
                uint32_t match_qty = std::min(taker_order->quantity, maker_order->quantity);
                
                if (report_queue_) {
                    TradeReport report{
                        .trade_id = trade_id_counter_++,
                        .maker_id = maker_order->order_id,
                        .taker_id = taker_order->order_id,
                        .price = level.price,
                        .quantity = match_qty,
                        .instrument_id = instrument_id_
                    };
                    report_queue_->push(report);
                }
                
                taker_order->quantity -= match_qty;
                maker_order->quantity -= match_qty;
                level.total_quantity -= match_qty;

                if (maker_order->quantity == 0) {
                    Order* to_release = maker_order;
                    level.remove_order(to_release);
                    order_map_[to_release->order_id % MaxOrders] = nullptr;
                    // Caller must release back to pool
                    maker_order = level.head;
                } else {
                    maker_order = maker_order->next;
                }
            }

            if (level.total_quantity == 0) {
                std::move(levels.begin() + level_idx + 1, levels.begin() + *num_levels, levels.begin() + level_idx);
                (*num_levels)--;
            } else {
                level_idx++;
            }
        }
    }

    template <typename Comparator>
    void add_to_book(Order* order, std::array<PriceLevel, MaxLevels>& levels, size_t* num_levels, Comparator comp) {
        auto it = std::lower_bound(levels.begin(), levels.begin() + *num_levels, order->price, 
            [&](const PriceLevel& lvl, int64_t price) { return comp(lvl.price, price); });

        size_t idx = std::distance(levels.begin(), it);
        if (idx < *num_levels && levels[idx].price == order->price) {
            levels[idx].add_order(order);
        } else if (*num_levels < MaxLevels) {
            std::move_backward(levels.begin() + idx, levels.begin() + *num_levels, levels.begin() + *num_levels + 1);
            levels[idx] = PriceLevel{order->price, 0, nullptr, nullptr};
            levels[idx].add_order(order);
            (*num_levels)++;
        }
    }

    uint32_t instrument_id_;
    std::array<PriceLevel, MaxLevels> bid_levels_;
    std::array<PriceLevel, MaxLevels> ask_levels_;
    size_t num_bids_ = 0;
    size_t num_asks_ = 0;
    uint64_t trade_id_counter_ = 0;

    std::array<Order*, MaxOrders> order_map_{}; 
    SPSCQueue<TradeReport, 1024>* report_queue_;
};

} // namespace nanomatch
