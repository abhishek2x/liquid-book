# liquid-book

### Low-Latency Order Book & Matching Engine (C++20)

A cache-optimized, lock-free limit order book and matching engine built from scratch in C++20. Focused entirely on systems engineering: data structure design, concurrent programming, and measured performance — no trading strategy logic lives here (see [`avellaneda-mm`](https://github.com/abhishek2x/avellaneda-mm) for the strategy layer, which depends on this repo).

> **Status: early development.** This README describes the target design. The "Benchmark Results" section will only be filled in with real numbers once code exists and benchmarks have actually run — see [`docs/description.md`](docs/description.md) for the honest, current build status and step-by-step plan.

---
> **Note on Strategy Execution:** To see this matching engine in action processing algorithmic trading flow, check out my implementation of an [Avellaneda-Stoikov Market Maker](https://github.com/abhishek2x/avellaneda-mm).
---

## Why this exists

Most portfolio projects that claim "low-latency" either (a) never measure anything, or (b) measure once and never re-verify after changes. This repo is an exercise in doing it properly: correctness first (tested against a reference implementation), concurrency safety second (ThreadSanitizer-clean), performance third (benchmarked, not guessed) — in that order, with each phase committed separately so the history is honest.

---

## Architecture

```
┌───────────────────────────────────────────────────────────────┐
│                      liquid-book (C++20)                      │
│                                                                 │
│   Market Replay ──► Order Book ──► FastQueue ──► Consumers     │
│   (L2 CSV Feed)     (Reverse Vec)   (Lock-Free    (Strategy /  │
│                                       SPMC Ring)    Risk /      │
│                                                      Logging)   │
│                          │                                      │
│                          ▼                                      │
│                    Sim Exchange                                 │
│                    (Queue-Priority Fill Model)                  │
└───────────────────────────────────────────────────────────────┘
```

## Core Components

| Component | What It Does | Key Design Choice |
|---|---|---|
| **Reverse Vector Order Book** | Maintains L2 depth for bid/ask sides | Best bid/ask mapped to vector back → O(1) frontier updates |
| **Lock-Free FastQueue** | Fans out book events to consumer threads | Bounded SPMC ring buffer, `alignas(64)` cache-line padding per consumer cursor |
| **Sim Exchange** | Models order matching against historical/synthetic data | Queue-priority-aware fill simulation |
| **Market Replay** | Feeds historical L2 data through the pipeline | Deterministic, reproducible test harness |
| **Risk Guards** | Enforces position limits and drawdown controls | Kill switch halts engine on breach |

## Repository Structure

```
include/
├── book/                   # Reverse Vector Order Book (L2)
├── queue/                  # Lock-free SPMC Ring Buffer (FastQueue)
└── risk/                   # Position limits, drawdown, kill switch
src/
├── engine.cpp              # Main event loop & orchestration
├── replay.cpp              # Market replay reader
└── simulator.cpp           # Simulated exchange / fill model
bench/                      # Micro-benchmarks (Google Benchmark)
tests/                      # Unit + integration tests, incl. TSan stress tests
data/                       # Sample L2 market data
docs/
└── description.md          # Detailed dev plan / progress log (start here)
CMakeLists.txt
```

## Prerequisites

- C++20 compiler — GCC 12+ / Clang 15+
- CMake ≥ 3.22
- Google Benchmark (fetched via CMake `FetchContent`)
- Clang with `-fsanitize=thread` support (for concurrency tests)

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

## Run Tests & Benchmarks

```bash
# Unit + correctness tests
ctest --test-dir build --output-on-failure

# ThreadSanitizer stress test (separate build)
cmake -B build-tsan -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-fsanitize=thread"
cmake --build build-tsan -j$(nproc)
./build-tsan/tests/queue_stress_test

# Micro-benchmarks (Release build only — never benchmark a Debug build)
./build/bench/book_benchmark
./build/bench/queue_benchmark
```

## Performance Targets

*(Not yet measured — placeholders until benchmarks are run and committed. Do not treat these as claims.)*

| Operation | Target Latency | Status |
|---|---|---|
| L2 book update (single level) | < 50 ns | ⏳ Not yet benchmarked |
| FastQueue publish (producer) | < 30 ns | ⏳ Not yet benchmarked |
| FastQueue consume (per reader) | < 40 ns | ⏳ Not yet benchmarked |
| End-to-end tick-to-book | < 1 μs | ⏳ Not yet benchmarked |

Once real numbers exist, this table moves to **"Benchmark Results"** and includes p50/p99/p99.9, hardware spec, and the exact commit the numbers came from.

## Performance Engineering Techniques (planned)

- **Zero allocations** in the hot path — memory pre-allocated at startup via contiguous arrays / object pools.
- **Cache-line isolation** — atomic cursors padded to 64 bytes (`alignas(64)`) to eliminate false sharing.
- **Acquire/release memory ordering** on the ring buffer — not `seq_cst` — with a written justification of why it's correct (see `docs/description.md`).
- **Branchless dispatch** where profiling shows branch mispredictions matter — not applied speculatively.

## Build Log / Progress

See [`docs/description.md`](docs/description.md) for the phase-by-phase plan and current status. This is updated as each phase actually lands, not in advance.

## License

MIT — see `LICENSE`.
