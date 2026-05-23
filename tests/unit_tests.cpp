#include <iostream>
#include <vector>
#include <memory>
#include <benchmark/benchmark.h>
#include "nanomatch/core/order_book.hpp"
#include "nanomatch/utils/object_pool.hpp"
#include "nanomatch/utils/tsc.hpp"

using namespace nanomatch;

#define MM_EXPECT(condition, msg) \
    if (!(condition)) { \
        std::cerr << "TEST FAILED: " << msg << " at " << __FILE__ << ":" << __LINE__ << "\n"; \
        std::exit(1); \
    }

class EngineTest {
public:
    void run_all() {
        test_price_time_priority();
        test_market_order_sweep();
        test_cancellation_o1();
        test_partial_fill();
        test_trade_reports_spsc();
        std::cout << "All Unit Tests Passed Successfully!\n";
    }

private:
    void test_price_time_priority() {
        auto pool = std::make_unique<ObjectPool<Order, 100>>();
        OrderBook<100, 100> ob(1, pool.get());

        Order* o1 = pool->acquire(); *o1 = {};
        o1->order_id = 1; o1->price = 100; o1->quantity = 10; o1->side = Side::SELL; o1->type = OrderType::LIMIT;
        Order* o2 = pool->acquire(); *o2 = {};
        o2->order_id = 2; o2->price = 100; o2->quantity = 10; o2->side = Side::SELL; o2->type = OrderType::LIMIT;
        
        ob.add_order(o1);
        ob.add_order(o2);

        Order* t1 = pool->acquire(); *t1 = {};
        t1->order_id = 3; t1->price = 100; t1->quantity = 5; t1->side = Side::BUY; t1->type = OrderType::LIMIT;
        ob.add_order(t1);

        MM_EXPECT(o1->quantity == 5, "Time priority: First order should be partially filled");
        MM_EXPECT(o2->quantity == 10, "Time priority: Second order should be untouched");
    }

    void test_market_order_sweep() {
        auto pool = std::make_unique<ObjectPool<Order, 100>>();
        OrderBook<100, 100> ob(1, pool.get());

        Order* o1 = pool->acquire(); *o1 = {};
        o1->order_id = 1; o1->price = 100; o1->quantity = 10; o1->side = Side::SELL; o1->type = OrderType::LIMIT;
        Order* o2 = pool->acquire(); *o2 = {};
        o2->order_id = 2; o2->price = 110; o2->quantity = 10; o2->side = Side::SELL; o2->type = OrderType::LIMIT;
        ob.add_order(o1);
        ob.add_order(o2);

        Order* m1 = pool->acquire(); *m1 = {};
        m1->order_id = 3; m1->quantity = 15; m1->side = Side::BUY; m1->type = OrderType::MARKET;
        ob.add_order(m1);

        MM_EXPECT(o1->quantity == 0, "Market order should sweep first level");
        MM_EXPECT(o2->quantity == 5, "Market order should partially sweep second level");
    }

    void test_cancellation_o1() {
        auto pool = std::make_unique<ObjectPool<Order, 100>>();
        OrderBook<100, 100> ob(1, pool.get());

        Order* o1 = pool->acquire(); *o1 = {};
        o1->order_id = 1; o1->price = 100; o1->quantity = 10; o1->side = Side::SELL; o1->type = OrderType::LIMIT;
        ob.add_order(o1);
        
        ob.cancel_order(1);
        
        Order* t1 = pool->acquire(); *t1 = {};
        t1->order_id = 2; t1->price = 100; t1->quantity = 10; t1->side = Side::BUY; t1->type = OrderType::LIMIT;
        ob.add_order(t1);
        MM_EXPECT(t1->quantity == 10, "Cancelled order should not be matched");
    }

    void test_partial_fill() {
        auto pool = std::make_unique<ObjectPool<Order, 100>>();
        OrderBook<100, 100> ob(1, pool.get());

        Order* o1 = pool->acquire(); *o1 = {};
        o1->order_id = 1; o1->price = 100; o1->quantity = 10; o1->side = Side::SELL; o1->type = OrderType::LIMIT;
        ob.add_order(o1);

        Order* t1 = pool->acquire(); *t1 = {};
        t1->order_id = 2; t1->price = 100; t1->quantity = 15; t1->side = Side::BUY; t1->type = OrderType::LIMIT;
        ob.add_order(t1);

        MM_EXPECT(o1->quantity == 0, "Maker order should be fully filled");
        MM_EXPECT(t1->quantity == 5, "Taker order should have remainder resting in book");
    }

    void test_trade_reports_spsc() {
        auto pool = std::make_unique<ObjectPool<Order, 100>>();
        SPSCQueue<TradeReport, 1024> report_queue;
        OrderBook<100, 100> ob(1, pool.get(), &report_queue);

        Order* o1 = pool->acquire(); *o1 = {};
        o1->order_id = 101; o1->price = 500; o1->quantity = 50; o1->side = Side::SELL; o1->type = OrderType::LIMIT;
        ob.add_order(o1);

        Order* t1 = pool->acquire(); *t1 = {};
        t1->order_id = 202; t1->price = 500; t1->quantity = 20; t1->side = Side::BUY; t1->type = OrderType::LIMIT;
        ob.add_order(t1);

        auto report = report_queue.pop();
        MM_EXPECT(report.has_value(), "SPSC Queue should contain a trade report");
        MM_EXPECT(report->maker_id == 101, "Report maker_id mismatch");
        MM_EXPECT(report->taker_id == 202, "Report taker_id mismatch");
        MM_EXPECT(report->quantity == 20, "Report quantity mismatch");
        MM_EXPECT(report->price == 500, "Report price mismatch");
    }
};

int main() {
    EngineTest test;
    test.run_all();
    uint64_t start = rdtsc();
    for(int i=0; i<1000; ++i) benchmark::DoNotOptimize(i);
    uint64_t end = rdtsc();
    std::cout << "RDTSC Measurement demo: 1000 loop cycles = " << (end - start) << " cycles.\n";
    return 0;
}
