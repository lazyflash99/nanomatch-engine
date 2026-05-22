# NANOMATCH: Ultra-Low Latency Order Matching Engine

NANOMATCH is a high-performance C++20 Limit Order Book (LOB) designed for sub-microsecond deterministic execution. This project was developed as part of the FEC '26 DIY series to demonstrate systems engineering principles used in high-frequency trading (HFT), including hardware sympathy, zero-allocation memory management, and lock-free concurrency.

## Architectural Narrative

The engine is architected to eliminate non-deterministic latency spikes and maximize CPU throughput through absolute hardware sympathy.

### 1. Zero-Allocation Memory Model
The matching engine hot path contains zero calls to the operating system heap (malloc/new). All Order objects are managed via a pre-allocated Object Pool with a LIFO free-list, ensuring O(1) memory acquisition and preventing jitter caused by heap-locking or fragmentation.

### 2. Cache-Line Alignment and False Sharing Prevention
Core data structures are aligned to 64-byte cache line boundaries using `alignas(64)`. This ensures individual orders occupy distinct cache lines, preventing "False Sharing"—a condition where multiple cores fight for ownership of the same cache line, destroying performance.

### 3. Multi-Format Ingestion Pipeline
To fulfill the project's data source requirements, the engine supports three distinct ingestion modes:
- **Binary ITCH:** High-speed pointer casting over memory-mapped files (mmap).
- **PCAP Network Captures:** Integrated parser for stripping Ethernet, IP, and UDP headers from network recordings.
- **Large-Scale CSV:** High-performance CSV parsing using `std::string_view` for multi-million row datasets.

### 4. Lock-Free Concurrency
Asynchronous trade reporting is handled via a Single-Producer Single-Consumer (SPSC) ring buffer. Using `acquire-release` memory ordering, the matching thread offloads I/O tasks to a background logger thread without the overhead of mutexes or kernel context switches.

## Performance Benchmark Report

Head-to-head comparison between the NANOMATCH optimized engine and a standard STL-based baseline (using `std::map` and `std::shared_ptr`).

*Test Environment: 12-Core @ 4400 MHz, Linux Kernel 5.15+, GCC 11.4.0 (-O3 -march=native -flto)*

| Metric | STL Baseline | NANOMATCH (Optimized) | Improvement |
| :--- | :--- | :--- | :--- |
| **Add Order (p50)** | 70.6 ns | **9.7 ns** | **~7.2x Faster** |
| **Throughput (Max)** | 20.5M orders/sec | **105.1M orders/sec** | **~5.1x Higher** |
| **Tick-to-Trade** | 55.8 CPU Cycles | **17.1 CPU Cycles** | **~3.2x More Efficient** |
| **Stability (StdDev)** | 14.4 ns | **0.9 ns** | **~16x Less Jitter** |

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
Build the engine and benchmark suite in Release mode for maximum optimization:
```bash
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j
```

### 4. Running Benchmarks
```bash
./nanomatch_bench --benchmark_display_aggregates_only=true
```

### 5. Executing the Engine
The engine automatically detects the input format (CSV, Binary, or PCAP):
```bash
./nanomatch_engine ../data_normal.csv
./nanomatch_engine ../data_crash.pcap
```

### 6. Visual Profiling (Flame Graph)
Generate the `perf.svg` to verify the absence of memory management overhead:
```bash
# 1. Record samples while running a dataset
sudo perf record -F 99 -g -- ./nanomatch_engine ../data_hft.bin
sudo chown $USER:$USER perf.data

# 2. Generate the Flame Graph (Requires Brendan Gregg's scripts in root)
git clone https://github.com/brendangregg/FlameGraph.git
perf script | ./FlameGraph/stackcollapse-perf.pl | ./FlameGraph/flamegraph.pl > ../perf.svg
```
The resulting `perf.svg` (included in this repository) shows the engine spending near 100% of its time in matching logic, proving successful hardware optimization.
