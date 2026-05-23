#include <benchmark/benchmark.h>
#include "nanomatch/core/order_book.hpp"
#include "nanomatch/core/stl_order_book.hpp"
#include "nanomatch/utils/object_pool.hpp"
#include "nanomatch/utils/tsc.hpp"
#include <vector>
#include <memory>
#include <algorithm>
#include <random>

using namespace nanomatch;

struct BenchData {
    std::vector<int64_t> prices;
    std::vector<Side> sides;
    
    BenchData(size_t n) {
        std::mt19937 gen(42);
        std::uniform_int_distribution<int64_t> price_dist(9900, 10100);
        std::uniform_int_distribution<int> side_dist(0, 1);
        for (size_t i = 0; i < n; ++i) {
            prices.push_back(price_dist(gen));
            sides.push_back(static_cast<Side>(side_dist(gen)));
        }
    }
};

void CustomStatistics(benchmark::internal::Benchmark* b) {
    b->ComputeStatistics("p50", [](const std::vector<double>& v) {
        std::vector<double> copy = v; std::sort(copy.begin(), copy.end());
        return copy[copy.size() / 2];
    });
    b->ComputeStatistics("p90", [](const std::vector<double>& v) {
        std::vector<double> copy = v; std::sort(copy.begin(), copy.end());
        return copy[static_cast<size_t>(copy.size() * 0.9)];
    });
    b->ComputeStatistics("p99", [](const std::vector<double>& v) {
        std::vector<double> copy = v; std::sort(copy.begin(), copy.end());
        return copy[static_cast<size_t>(copy.size() * 0.99)];
    });
}

static void BM_Optimized_RealWorld(benchmark::State& state) {
    auto pool = std::make_unique<ObjectPool<Order, 1000000>>();
    auto ob = std::make_unique<OrderBook<1024, 1000000>>(1, pool.get());
    BenchData data(1000000);
    
    for (int i = 0; i < 500; ++i) {
        Order* o = pool->acquire();
        *o = {};
        o->order_id = static_cast<uint64_t>(i);
        o->price = 10000 + (i % 50);
        o->quantity = 100;
        o->instrument_id = 1;
        o->side = Side::SELL;
        o->type = OrderType::LIMIT;
        ob->add_order(o);
    }

    size_t i = 0;
    uint64_t order_id = 1000;
    uint64_t total_cycles = 0;

    for (auto _ : state) {
        Order* order = pool->acquire();
        if(!order) break;
        *order = {};
        order->order_id = order_id++;
        order->price = data.prices[i % 1000000];
        order->quantity = 10;
        order->instrument_id = 1;
        order->side = data.sides[i % 1000000];
        order->type = OrderType::LIMIT;
        
        uint64_t start = rdtsc();
        ob->add_order(order);
        uint64_t end = rdtsc();
        
        total_cycles += (end - start);
        benchmark::DoNotOptimize(ob);
        i++;
    }
    state.counters["AvgCycles"] = static_cast<double>(total_cycles) / state.iterations();
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Optimized_RealWorld)->Iterations(500000)->Unit(benchmark::kNanosecond)->Apply(CustomStatistics)->Repetitions(5);

static void BM_STL_RealWorld(benchmark::State& state) {
    auto ob = std::make_unique<STLOrderBook>(1);
    BenchData data(1000000);
    for (int i = 0; i < 500; ++i) ob->add_order(i, 10000 + (i % 50), 100, Side::SELL, OrderType::LIMIT);

    size_t i = 0;
    uint64_t order_id = 1000;
    uint64_t total_cycles = 0;

    for (auto _ : state) {
        uint64_t start = rdtsc();
        ob->add_order(order_id++, data.prices[i % 1000000], 10, data.sides[i % 1000000], OrderType::LIMIT);
        uint64_t end = rdtsc();
        
        total_cycles += (end - start);
        benchmark::DoNotOptimize(ob);
        i++;
    }
    state.counters["AvgCycles"] = static_cast<double>(total_cycles) / state.iterations();
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_STL_RealWorld)->Unit(benchmark::kNanosecond)->Apply(CustomStatistics)->Repetitions(5);

BENCHMARK_MAIN();
