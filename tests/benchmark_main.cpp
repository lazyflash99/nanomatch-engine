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

// --- REAL-WORLD BENCHMARK DATA GENERATOR ---
// Pre-generating data to ensure the randomizer doesn't skew the benchmark time
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
        std::vector<double> copy = v;
        std::sort(copy.begin(), copy.end());
        return copy[copy.size() / 2];
    });
    b->ComputeStatistics("p99", [](const std::vector<double>& v) {
        std::vector<double> copy = v;
        std::sort(copy.begin(), copy.end());
        return copy[static_cast<size_t>(copy.size() * 0.99)];
    });
}

// --- OPTIMIZED ENGINE: REAL-WORLD SCENARIO ---
static void BM_Optimized_RealWorld(benchmark::State& state) {
    auto pool = std::make_unique<ObjectPool<Order, 1000000>>();
    auto ob = std::make_unique<OrderBook<1024, 1000000>>(1);
    BenchData data(1000000);
    
    // 1. Pre-fill the book with 500 levels to simulate depth
    for (int i = 0; i < 500; ++i) {
        Order* o = pool->acquire();
        o->order_id = i;
        o->price = 10000 + (i % 50);
        o->quantity = 100;
        o->side = Side::SELL;
        o->type = OrderType::LIMIT;
        ob->add_order(o);
    }

    size_t i = 0;
    uint64_t order_id = 1000;
    
    for (auto _ : state) {
        Order* order = pool->acquire();
        if(!order) break;
        
        // Randomize to break branch prediction
        order->order_id = order_id++;
        order->price = data.prices[i % 1000000];
        order->side = data.sides[i % 1000000];
        order->quantity = 10;
        order->type = OrderType::LIMIT;

        ob->add_order(order);
        
        benchmark::DoNotOptimize(ob);
        i++;
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Optimized_RealWorld)->Iterations(500000)->Unit(benchmark::kNanosecond)->Apply(CustomStatistics)->Repetitions(5);

// --- STL ENGINE: REAL-WORLD SCENARIO ---
static void BM_STL_RealWorld(benchmark::State& state) {
    auto ob = std::make_unique<STLOrderBook>(1);
    BenchData data(1000000);

    // 1. Pre-fill
    for (int i = 0; i < 500; ++i) {
        ob->add_order(i, 10000 + (i % 50), 100, Side::SELL, OrderType::LIMIT);
    }

    size_t i = 0;
    uint64_t order_id = 1000;

    for (auto _ : state) {
        ob->add_order(order_id++, data.prices[i % 1000000], 10, data.sides[i % 1000000], OrderType::LIMIT);
        benchmark::DoNotOptimize(ob);
        i++;
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_STL_RealWorld)->Unit(benchmark::kNanosecond)->Apply(CustomStatistics)->Repetitions(5);

BENCHMARK_MAIN();
