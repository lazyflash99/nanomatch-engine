#pragma once
#include <map>
#include <list>
#include <unordered_map>
#include <memory>
#include <functional>
#include <algorithm>
#include "nanomatch/core/types.hpp"

namespace nanomatch {

struct STLOrder {
    uint64_t order_id;
    int64_t price;
    uint32_t quantity;
    uint32_t instrument_id;
    Side side;
    OrderType type;
};

class STLOrderBook {
public:
    STLOrderBook(uint32_t inst_id) : instrument_id_(inst_id) {}

    void add_order(uint64_t id, int64_t price, uint32_t qty, Side side, OrderType type) {
        auto order = std::make_shared<STLOrder>(STLOrder{id, price, qty, instrument_id_, side, type});
        
        if (side == Side::BUY) {
            match(order, asks_, std::less_equal<int64_t>{});
            if (order->quantity > 0 && order->type == OrderType::LIMIT) {
                bids_[price].push_back(order);
                order_map_[id] = --bids_[price].end();
            }
        } else {
            match(order, bids_, std::greater_equal<int64_t>{});
            if (order->quantity > 0 && order->type == OrderType::LIMIT) {
                asks_[price].push_back(order);
                order_map_[id] = --asks_[price].end();
            }
        }
    }

    void cancel_order(uint64_t order_id) {
        auto it = order_map_.find(order_id);
        if (it != order_map_.end()) {
            auto order = *it->second;
            if (order->side == Side::BUY) {
                bids_[order->price].erase(it->second);
                if (bids_[order->price].empty()) bids_.erase(order->price);
            } else {
                asks_[order->price].erase(it->second);
                if (asks_[order->price].empty()) asks_.erase(order->price);
            }
            order_map_.erase(it);
        }
    }

private:
    template <typename MapType, typename Comparator>
    void match(std::shared_ptr<STLOrder> taker, MapType& book, Comparator can_match) {
        for (auto it = book.begin(); it != book.end() && taker->quantity > 0; ) {
            if (taker->type == OrderType::LIMIT && !can_match(taker->price, it->first)) break;

            auto& queue = it->second;
            for (auto q_it = queue.begin(); q_it != queue.end() && taker->quantity > 0; ) {
                auto maker = *q_it;
                uint32_t match_qty = std::min(taker->quantity, maker->quantity);
                
                taker->quantity -= match_qty;
                maker->quantity -= match_qty;

                if (maker->quantity == 0) {
                    order_map_.erase(maker->order_id);
                    q_it = queue.erase(q_it);
                } else {
                    ++q_it;
                }
            }

            if (queue.empty()) {
                it = book.erase(it);
            } else {
                ++it;
            }
        }
    }

    uint32_t instrument_id_;
    std::map<int64_t, std::list<std::shared_ptr<STLOrder>>, std::greater<int64_t>> bids_;
    std::map<int64_t, std::list<std::shared_ptr<STLOrder>>, std::less<int64_t>> asks_;
    std::unordered_map<uint64_t, std::list<std::shared_ptr<STLOrder>>::iterator> order_map_;
};

} // namespace nanomatch
