#pragma once

#include "nanomatch/core/types.hpp"
#include "nanomatch/utils/object_pool.hpp"
#include "nanomatch/utils/spsc_queue.hpp"
#include <algorithm>
#include <array>
#include <functional>
#include <cstddef>

namespace nanomatch {

struct PriceLevel {
    int64_t price = 0;
    uint32_t total_quantity = 0;
    Order* head = nullptr;
    Order* tail = nullptr;

    void add_order(Order* order) {
        order->level = this;
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
        order->level = nullptr;
    }
};

template <typename T, size_t Capacity>
class OrderMap {
public:
    static constexpr uint64_t TOMBSTONE = UINT64_MAX;

    struct Entry {
        uint64_t id = 0;
        T* ptr = nullptr;
    };

    void insert(uint64_t id, T* ptr) {
        size_t idx = id % Capacity;
        size_t start = idx;
        size_t tomb_idx = Capacity;

        while (entries_[idx].id != 0 && entries_[idx].id != id) {
            if (entries_[idx].id == TOMBSTONE && tomb_idx == Capacity) tomb_idx = idx;
            idx = (idx + 1) % Capacity;
            if (idx == start) break;
        }

        if (entries_[idx].id == id) {
            entries_[idx].ptr = ptr;
        } else if (tomb_idx != Capacity) {
            entries_[tomb_idx] = {id, ptr};
        } else {
            entries_[idx] = {id, ptr};
        }
    }

    T* find(uint64_t id) {
        size_t idx = id % Capacity;
        size_t start = idx;
        while (entries_[idx].id != 0) {
            if (entries_[idx].id == id) return entries_[idx].ptr;
            idx = (idx + 1) % Capacity;
            if (idx == start) break;
        }
        return nullptr;
    }

    void erase(uint64_t id) {
        size_t idx = id % Capacity;
        size_t start = idx;
        while (entries_[idx].id != 0) {
            if (entries_[idx].id == id) {
                entries_[idx].id = TOMBSTONE;
                entries_[idx].ptr = nullptr;
                return;
            }
            idx = (idx + 1) % Capacity;
            if (idx == start) break;
        }
    }

private:
    std::array<Entry, Capacity> entries_{};
};

template <size_t MaxLevels = 1024, size_t MaxOrders = 200000>
class OrderBook {
public:
    OrderBook(uint32_t instrument_id, 
             ObjectPool<Order, MaxOrders>* pool,
             SPSCQueue<TradeReport, 1024>* report_queue = nullptr) 
        : instrument_id_(instrument_id), pool_(pool), report_queue_(report_queue) {}

    void add_order(Order* order) {
        if (order->side == Side::BUY) {
            match(order, ask_levels_, &num_asks_, std::less_equal<int64_t>{});
            if (order->quantity > 0 && order->type == OrderType::LIMIT) {
                add_to_book(order, bid_levels_, &num_bids_, std::greater<int64_t>{});
                order_map_.insert(order->order_id, order);
            } else {
                pool_->release(order);
            }
        } else {
            match(order, bid_levels_, &num_bids_, std::greater_equal<int64_t>{});
            if (order->quantity > 0 && order->type == OrderType::LIMIT) {
                add_to_book(order, ask_levels_, &num_asks_, std::less<int64_t>{});
                order_map_.insert(order->order_id, order);
            } else {
                pool_->release(order);
            }
        }
    }

    void cancel_order(uint64_t order_id) {
        Order* order = order_map_.find(order_id);
        if (!order) return;

        if (order->level) {
            PriceLevel* level = order->level;
            level->remove_order(order);
            if (level->total_quantity == 0) {
                auto& levels = (order->side == Side::BUY) ? bid_levels_ : ask_levels_;
                auto& count = (order->side == Side::BUY) ? num_bids_ : num_asks_;
                for (size_t i = 0; i < count; ++i) {
                    if (&levels[i] == level) {
                        std::move(levels.begin() + i + 1, levels.begin() + count, levels.begin() + i);
                        count--;
                        break;
                    }
                }
            }
        }
        order_map_.erase(order_id);
        pool_->release(order);
    }

private:
    template <typename Comparator>
    void match(Order* taker, std::array<PriceLevel, MaxLevels>& levels, size_t* num_levels, Comparator can_match) {
        size_t level_idx = 0;
        while (taker->quantity > 0 && level_idx < *num_levels) {
            auto& level = levels[level_idx];
            if (taker->type == OrderType::LIMIT && !can_match(taker->price, level.price)) break;

            Order* maker = level.head;
            while (maker && taker->quantity > 0) {
                uint32_t match_qty = std::min(taker->quantity, maker->quantity);
                if (match_qty == 0) break;

                if (report_queue_) {
                    report_queue_->push(TradeReport{trade_id_++, maker->order_id, taker->order_id, level.price, match_qty, instrument_id_});
                }
                
                taker->quantity -= match_qty;
                maker->quantity -= match_qty;
                level.total_quantity -= match_qty;

                if (maker->quantity == 0) {
                    Order* to_release = maker;
                    level.remove_order(to_release);
                    order_map_.erase(to_release->order_id);
                    maker = level.head;
                    pool_->release(to_release);
                } else {
                    maker = maker->next;
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
    ObjectPool<Order, MaxOrders>* pool_;
    std::array<PriceLevel, MaxLevels> bid_levels_;
    std::array<PriceLevel, MaxLevels> ask_levels_;
    size_t num_bids_ = 0;
    size_t num_asks_ = 0;
    uint64_t trade_id_ = 1;
    OrderMap<Order, MaxOrders * 2> order_map_; 
    SPSCQueue<TradeReport, 1024>* report_queue_;
};

} // namespace nanomatch
