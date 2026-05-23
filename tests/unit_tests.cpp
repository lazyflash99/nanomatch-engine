#include <iostream>
#include <cassert>
#include <benchmark/benchmark.h>
#include "nanomatch/core/order_book.hpp"
#include "nanomatch/utils/object_pool.hpp"
#include "nanomatch/utils/tsc.hpp"

using namespace nanomatch;

class EngineTest {
public:
    void run_all() {
        test_price_time_priority();
        test_market_order_sweep();
        test_cancellation_o1();
        test_partial_fill();
        std::cout << "All Unit Tests Passed Successfully!\n";
    }

private:
    void test_price_time_priority() {
        ObjectPool<Order, 100> pool;
        OrderBook<100, 100> ob(1, &pool);

        Order* o1 = pool.acquire(); 
        *o1 = {};
        o1->order_id = 1; o1->price = 100; o1->quantity = 10; o1->instrument_id = 1; o1->side = Side::SELL; o1->type = OrderType::LIMIT;
        
        Order* o2 = pool.acquire(); 
        *o2 = {};
        o2->order_id = 2; o2->price = 100; o2->quantity = 10; o2->instrument_id = 1; o2->side = Side::SELL; o2->type = OrderType::LIMIT;
        
        ob.add_order(o1);
        ob.add_order(o2);

        Order* t1 = pool.acquire(); 
        *t1 = {};
        t1->order_id = 3; t1->price = 100; t1->quantity = 5; t1->instrument_id = 1; t1->side = Side::BUY; t1->type = OrderType::LIMIT;
        ob.add_order(t1);

        assert(o1->quantity == 5);
        assert(o2->quantity == 10);
    }

    void test_market_order_sweep() {
        ObjectPool<Order, 100> pool;
        OrderBook<100, 100> ob(1, &pool);

        Order* o1 = pool.acquire(); 
        *o1 = {};
        o1->order_id = 1; o1->price = 100; o1->quantity = 10; o1->instrument_id = 1; o1->side = Side::SELL; o1->type = OrderType::LIMIT;
        
        Order* o2 = pool.acquire(); 
        *o2 = {};
        o2->order_id = 2; o2->price = 110; o2->quantity = 10; o2->instrument_id = 1; o2->side = Side::SELL; o2->type = OrderType::LIMIT;
        
        ob.add_order(o1);
        ob.add_order(o2);

        Order* m1 = pool.acquire(); 
        *m1 = {};
        m1->order_id = 3; m1->price = 0; m1->quantity = 15; m1->instrument_id = 1; m1->side = Side::BUY; m1->type = OrderType::MARKET;
        ob.add_order(m1);

        assert(o1->quantity == 0);
        assert(o2->quantity == 5);
    }

    void test_cancellation_o1() {
        ObjectPool<Order, 100> pool;
        OrderBook<100, 100> ob(1, &pool);

        Order* o1 = pool.acquire(); 
        *o1 = {};
        o1->order_id = 1; o1->price = 100; o1->quantity = 10; o1->instrument_id = 1; o1->side = Side::SELL; o1->type = OrderType::LIMIT;
        ob.add_order(o1);
        
        ob.cancel_order(1);
        
        Order* t1 = pool.acquire(); 
        *t1 = {};
        t1->order_id = 2; t1->price = 100; t1->quantity = 10; t1->instrument_id = 1; t1->side = Side::BUY; t1->type = OrderType::LIMIT;
        ob.add_order(t1);
        assert(t1->quantity == 10);
    }

    void test_partial_fill() {
        ObjectPool<Order, 100> pool;
        OrderBook<100, 100> ob(1, &pool);

        Order* o1 = pool.acquire(); 
        *o1 = {};
        o1->order_id = 1; o1->price = 100; o1->quantity = 10; o1->instrument_id = 1; o1->side = Side::SELL; o1->type = OrderType::LIMIT;
        ob.add_order(o1);

        Order* t1 = pool.acquire(); 
        *t1 = {};
        t1->order_id = 2; t1->price = 100; t1->quantity = 15; t1->instrument_id = 1; t1->side = Side::BUY; t1->type = OrderType::LIMIT;
        ob.add_order(t1);

        assert(o1->quantity == 0);
        assert(t1->quantity == 5);
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
