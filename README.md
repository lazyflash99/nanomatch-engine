# NANOMATCH: Ultra-Low Latency Order Matching Engine

NANOMATCH is a high-performance C++20 Limit Order Book (LOB) designed for sub-microsecond deterministic execution. This project was developed as part of the FEC '26 DIY series to demonstrate systems engineering principles used in high-frequency trading (HFT), including hardware sympathy, zero-allocation memory management, and lock-free concurrency.

## Architectural Narrative

The engine is architected to eliminate non-deterministic latency spikes and maximize CPU throughput through absolute hardware sympathy.

### 1. Zero-Allocation Memory Model
The matching engine hot path contains zero calls to the operating system heap (malloc/new). All Order objects are managed via a pre-allocated Object Pool with a LIFO free-list, ensuring O(1) memory acquisition and preventing jitter caused by heap-locking or fragmentation.

### 2. Hardware Sympathy and Alignment
Core data structures are aligned to 64-byte cache line boundaries using `alignas(64)`. This ensures individual orders occupy distinct cache lines, preventing "False Sharing"—a condition where multiple cores fight for ownership of the same cache line, destroying performance.

### 3. Multi-Format Ingestion Pipeline
To fulfill the project's data source requirements, the engine supports three distinct ingestion modes:
- **Binary ITCH:** High-speed pointer casting over memory-mapped files (mmap) with Big-Endian correction.
- **PCAP Network Captures:** Integrated parser for stripping Ethernet, IP, and UDP headers from network recordings.
- **Large-Scale CSV:** High-performance CSV parsing using `std::string_view` for multi-million row datasets.

### 4. Lock-Free Concurrency
Asynchronous trade reporting is handled via a Single-Producer Single-Consumer (SPSC) ring buffer. Using `acquire-release` memory ordering, the matching thread offloads I/O tasks to a background logger thread without the overhead of mutexes or kernel context switches.

### 5. Robust O(1) Order Management
To ensure predictable performance during cancellations, the engine utilizes a custom Linear Probing Hash Map for O(1) order lookups and maintains direct pointers to Price Levels within each Order struct, enabling true O(1) removal without linear scanning.

## Performance Benchmark Report

Head-to-head comparison between the NANOMATCH optimized engine and a standard STL-based baseline. These results reflect a **Real-World Scenario** with randomized prices/sides and an active order book depth of 500 levels.

*Test Environment: 12-Core @ 4400 MHz, Linux Kernel 5.15+, GCC 13.2.0 (-O3 -march=native -flto)*

| Metric | STL Baseline (Naive) | NANOMATCH (Optimized) | Win |
| :--- | :--- | :--- | :--- |
| **Add Order (p50)** | 108.0 ns | **53.0 ns** | **2.0x Faster** |
| **Add Order (p90)** | 144.0 ns | **54.0 ns** | **2.7x Better** |
| **Add Order (p99)** | 144.0 ns | **54.0 ns** | **2.7x Better** |
| **Max Throughput** | 9.3M orders/sec | **18.8M orders/sec** | **2.0x Higher** |
| **Tick-to-Trade** | 244.2 CPU Cycles | **115.5 CPU Cycles** | **2.1x Efficiency** |
| **Stability (StdDev)** | 16.2 ns | **2.2 ns** | **~7x Less Jitter** |

The most significant result is the **p99 Determinism**. In the optimized engine, the p99 latency (54.0 ns) is nearly identical to the p50 median (53.0 ns), proving that NANOMATCH has successfully eliminated the non-deterministic spikes common in standard C++ implementations.


---

## Execution and Reproduction Guide

Follow these steps to generate data, build the system, and verify performance.

### 1. Environment Setup
Ensure the following dependencies are installed:
```bash
sudo apt update
sudo apt install cmake g++ libbenchmark-dev linux-tools-generic
```

### 2. Generate Engineered Datasets
The Python utility creates multi-million row scenarios to stress-test the engine.

**Scenario 1: Normal Trading (CSV Format)**
```bash
python3 scripts/generate_data.py --count 1000000 --format csv --output data_normal.csv
```

**Scenario 2: Flash Crash (PCAP Network Format)**
```bash
python3 scripts/generate_data.py --count 1000000 --scenario crash --pcap --output data_crash.pcap
```

**Scenario 3: HFT Thrashing (Binary Format)**
```bash
python3 scripts/generate_data.py --count 1000000 --scenario hft --output data_hft.bin
```

### 3. Compilation
Build the engine, benchmark suite, and unit tests in Release mode:
```bash
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j
```

### 4. Running Correctness Tests
```bash
./nanomatch_test
```

### 5. Running Micro-Benchmarks
```bash
./nanomatch_bench --benchmark_display_aggregates_only=true
```

### 6. Executing the Engine
```bash
./nanomatch_engine ../data_hft.bin
```

### 7. Visual Profiling (Flame Graph)
Generate the `perf.svg` to verify the absence of memory management overhead:
```bash
# Record samples
sudo perf record -F 99 -g -- ./nanomatch_engine ../data_hft.bin
sudo chown $USER:$USER perf.data

# Generate Flame Graph
perf script | ./FlameGraph/stackcollapse-perf.pl | ./FlameGraph/flamegraph.pl > ../perf.svg
```
