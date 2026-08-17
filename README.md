# liquid-book

### Low-Latency Exchange Simulator, Order Book & Matching Engine (C++20)

A cache-optimized, low-latency exchange simulator built from scratch in **C++20**, featuring a lock-free limit order book, matching engine, lock-free event dispatcher, and event-driven market data pipeline. The project focuses on **systems engineering** — memory locality, lock-free concurrency, acquire/release memory semantics, and empirical benchmarking.

> **Companion Project:** To see this exchange processing live market-making orders, check out the **[Avellaneda-MM Market Maker](https://github.com/abhishek2x/avellaneda-mm)** (Python/C++ binding participant).

---

## Key Features

- **Reverse-Vector L2 Order Book**: Best bid/ask mapped to vector boundaries for $O(1)$ top-of-book updates with superior cache locality compared to tree-based maps (`std::map`).
- **Lock-Free SPMC FastQueue**: Single Producer, Multiple Consumer circular ring buffer with acquire-release semantics and 64-byte cache-line alignment to eliminate false sharing.
- **Deterministic Market Replay**: Replays timestamped L2 CSV market data for 100% reproducible execution and trade generation.
- **Exchange Risk Guards**: Real-time $O(1)$ net inventory and drawdown limits with atomic kill-switch interlocks.
- **Decoupled Execution Pipeline**: Multi-threaded architecture separating market event replay from risk analysis and downstream event processing.

---

## System Architecture

`liquid-book` behaves as a standalone, strategy-agnostic electronic exchange.

```text
                      Upstream Market Data / Replay
                                   │
                              Order Flow
                                   │
                                   ▼
        ┌─────────────────────────────────────────────────────┐
        │                 Liquid Book (C++)                   │
        │─────────────────────────────────────────────────────│
        │  Reverse Vector L2 Order Book                       │
        │  Simulated Matching Engine                          │
        │  Lock-Free SPMC Event Queue (FastQueue)             │
        │  Exchange Risk Guards & Kill Switch                 │
        └──────────────────────────┬──────────────────────────┘
                                   │
                ┌──────────────────┼──────────────────┐
                ▼                  ▼                  ▼
           Avellaneda-MM       TWAP Bot         Noise Trader
            (Python)           (C++)             (Future)
```

### Concurrent Execution Pipeline

The execution engine uses `FastQueue` to decouple event generation from event consumption:

```text
 [Producer Thread: MarketReplay]
              │
              ▼
   (Simulate L2 Updates)
              │
              ▼
       [OrderBook]
              │
        (If Crossed)
              ▼
       [SimExchange] ──(Generates Trade)──► [FastQueue] (Lock-Free SPMC)
                                                  │
                                                  │ (try_pop)
                                                  ▼
                                      [Consumer Thread: Risk & Logging]
                                                  │
                                                  ├──► [RiskGuard] (Check Limits)
                                                  └──► stdout (Print Trade)
```

---

## Core Components & Technical Documentation

Detailed design deep dives for each component are maintained in the [`docs/`](docs/) directory:

| Component | Responsibility | Primary Design Choice | Documentation |
| :--- | :--- | :--- | :--- |
| **Order Book** | Maintains L2 bid/ask depth | Reverse vector layout (`std::vector<PriceLevel>`) | [order_book.md](docs/order_book.md) |
| **Lock-Free Queue** | SPMC event dispatcher | Bounded ring buffer (`alignas(64)`, acquire/release) | [fast_queue.md](docs/fast_queue.md) |
| **SimExchange & Replay** | Matches crossings & replays CSVs | Deterministic aggregate-level matching | [sim_exchange.md](docs/sim_exchange.md) |
| **Risk Guards** | Exchange safety controls | $O(1)$ position & drawdown checks with kill switch | [risk_guards.md](docs/risk_guards.md) |
| **System Architecture** | Overall platform design | Strategy-independent exchange architecture | [architecture.md](docs/architecture.md) |
| **C++ Systems Concepts** | Low-latency engineering | Memory ordering, ODR, alignment, header separation | [cpp_concepts.md](docs/cpp_concepts.md) |
| **Development Plan** | Phase status & roadmap | Phase-by-phase implementation status | [description.md](docs/description.md) |

---

## Participant API Overview

Trading strategies communicate with the exchange via a clean participant interface:

```python
# Python Participant Interface (via pybind11)
exchange.place_limit_order(side="BUY", price=100.25, quantity=10)
exchange.cancel_order(order_id)

# Market Data API
best_bid = exchange.best_bid()
best_ask = exchange.best_ask()
book_depth = exchange.get_order_book()
```

---

## Performance & Benchmarks

Measured locally on Apple M2 (Apple Clang 21.0.0):

| Operation | Target Latency | Measured Performance |
| :--- | :--- | :--- |
| **FastQueue Push/Pop Cycle** | `< 30 ns` | **`~16.6 ns`** |
| **Order Book Best Bid/Ask Lookup** | `< 5 ns` | **`~1.4 ns`** |
| **Order Book Top-of-Book Update** | `< 50 ns` | **`~843 ns`** |
| **Order Book Mid-Book Update** | `< 100 ns` | **`~879 ns`** |
| **Order Deletion** | `< 100 ns` | **`~1027 ns`** |

*Note: Benchmarks collected via local release harnesses and Google Benchmark suite (`bench/`).*

---

## Build & Run

### 1. Build Requirements
- C++20 compliant compiler (GCC 11+, Clang 13+, Apple Clang 13+)
- CMake 3.22+

### 2. Build the Project
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### 3. Run Engine Simulation
```bash
./build/liquid_book_engine data/sample_l2.csv --max-pos 100
```

### 4. Run Test Suite
```bash
ctest --test-dir build --output-on-failure
```

### 5. Run ThreadSanitizer (TSan) Stress Tests
```bash
cmake -B build-tsan -DCMAKE_BUILD_TYPE=Debug -DENABLE_TSAN=ON
cmake --build build-tsan
./build-tsan/tests/queue_stress_test
```

### 6. Run Micro-benchmarks
```bash
./build/bench/queue_benchmark
./build/bench/book_benchmark
```

---

## Repository Structure

```text
include/
├── book/           # Reverse Vector L2 Order Book
├── queue/          # Lock-free SPMC Event Queue (FastQueue)
├── risk/           # Risk Limits & Kill Switch Guard
└── sim/            # SimExchange & Deterministic Market Replay

src/
├── book/           # Order book implementation
├── sim/            # Market replay & crossing matcher
└── engine.cpp      # Main executable entry point & concurrent pipeline

docs/               # Technical specifications & design docs
bench/              # Latency benchmarks
data/               # Sample L2 market data CSVs
tests/              # Unit & concurrency stress test suite
```

---

## License

MIT — see [LICENSE](LICENSE).
