# liquid-book

### Low-Latency Exchange Simulator, Order Book & Matching Engine (C++20)

A cache-optimized, low-latency exchange simulator built from scratch in **C++20**, featuring a lock-free limit order book, matching engine, and event-driven market data pipeline. The project focuses entirely on **systems engineering**—data structure design, concurrent programming, memory optimization, and measured performance.

Unlike most matching engine projects, **Liquid Book is designed as a reusable exchange simulator**. It exposes a clean participant API that allows trading strategies (written in Python, C++, or any future language binding) to connect, receive market data, and submit orders.

> **Status: Early Development.** This README describes the target architecture. Benchmark numbers will only be added once they have been measured and reproduced. See `docs/description.md` for the current implementation status and development roadmap.

---

> **Companion Project:** To see this exchange processing live market-making orders, check out my implementation of an **Avellaneda-Stoikov Market Maker**:
>
> https://github.com/abhishek2x/avellaneda-mm

---

# Why this exists

Most open-source "matching engines" stop at maintaining an order book.

Real electronic exchanges do much more:

- Maintain a limit order book
- Match incoming orders
- Generate trades
- Publish market data
- Notify participants of fills
- Support multiple independent trading participants
- Provide deterministic replay for testing

Liquid Book aims to model these exchange mechanics while remaining lightweight, deterministic, and performance-focused.

The goal is to build a reusable exchange engine that can power multiple algorithmic trading strategies without embedding any strategy-specific logic.

---

# System Architecture

```text
                      Upstream Market
               (Replay / Synthetic Traders)
                           │
                      Order Flow
                           │
                           ▼
        ┌─────────────────────────────────────┐
        │          Liquid Book (C++)          │
        │─────────────────────────────────────│
        │ Reverse Vector Order Book           │
        │ Matching Engine                     │
        │ Trade Engine                        │
        │ Event Bus                           │
        │ Market Data Feed                    │
        │ Participant API                     │
        └──────────────┬──────────────────────┘
                       │
      ┌────────────────┼────────────────┐
      │                │                │
      ▼                ▼                ▼
 Avellaneda-MM      TWAP Bot      Random Trader
    (Python)         (Future)       (Future)
```

Liquid Book behaves as a miniature electronic exchange.

Trading strategies are completely independent applications that connect through the exchange API.

---

# Companion Project

This repository intentionally contains **no trading logic**.

Strategies live outside the exchange.

Current participant:

- **Avellaneda-MM (Python)** — Inventory-aware market-making strategy implementing the Avellaneda-Stoikov model.

Future participants may include:

- TWAP execution
- VWAP execution
- Momentum strategies
- Reinforcement Learning agents
- Arbitrage strategies
- Noise traders

This separation mirrors production trading systems where exchanges remain strategy-agnostic.

---

# Component Architecture

```text
                    Incoming Orders
                           │
                           ▼
                 Reverse Vector Order Book
                           │
                           ▼
                    Matching Engine
                           │
              ┌────────────┴────────────┐
              ▼                         ▼
        Trade Generation         Order Book Update
              │                         │
              └────────────┬────────────┘
                           ▼
                    Event Dispatcher
                           │
          ┌────────────────┼────────────────┐
          ▼                ▼                ▼
     Market Data      Fill Events      Trade Stream
```

---

# Core Components

| Component | Responsibility | Design Choice |
|-----------|----------------|---------------|
| **Reverse Vector Order Book** | Maintains L2 bid/ask depth | Best price mapped to vector back for O(1) frontier updates |
| **Matching Engine** | Matches incoming orders | Price-time priority |
| **Trade Engine** | Generates executions | Deterministic trade generation |
| **Participant API** | Allows strategies to interact with the exchange | Language-agnostic interface (Python bindings initially) |
| **Event Dispatcher** | Broadcasts market events | Lock-free SPMC ring buffer |
| **Market Replay** | Replays historical market data | Deterministic testing |
| **Risk Guards** | Exchange-side safety controls | Position limits & kill switch |

---

# Participant API

Trading strategies communicate with the exchange using a simple participant interface.

### Order API

```python
exchange.place_limit_order(
    side="BUY",
    price=100.25,
    quantity=10
)

exchange.cancel_order(order_id)
```

### Market Data API

```python
book = exchange.get_order_book()

best_bid = exchange.best_bid()
best_ask = exchange.best_ask()

recent_trades = exchange.get_recent_trades()
```

### Event API (Planned)

```python
exchange.on_trade(callback)

exchange.on_book_update(callback)

exchange.on_fill(callback)
```

Initially, these APIs will be exposed to Python through **pybind11**, allowing strategies to use the C++ exchange directly without networking overhead.

Future communication layers may include:

- gRPC
- ZeroMQ
- Kafka / NATS
- Native C++ API

without changing exchange internals.

---

# Repository Structure

```text
include/
├── book/                   # Reverse Vector Order Book
├── matching/               # Matching Engine
├── exchange/               # Exchange & Participant API
├── queue/                  # Lock-free Event Queue
└── risk/                   # Risk Guards

src/
├── exchange.cpp
├── engine.cpp
├── replay.cpp
├── simulator.cpp

bench/
tests/
data/

docs/
└── description.md

CMakeLists.txt
```

---

# Design Principles

- Strategy-independent exchange
- Zero allocations in the hot path
- Cache-efficient memory layout
- Deterministic execution
- Lock-free event propagation
- Reproducible benchmarking
- Test-driven correctness before optimization

---

# Performance Engineering (Planned)

- Preallocated memory pools
- Cache-line padding (`alignas(64)`)
- Acquire/Release memory ordering
- False-sharing elimination
- Branch prediction optimization
- SIMD where beneficial
- Benchmark-driven optimization only

---

# Performance Targets

> Placeholder goals until actual benchmarks are committed.

| Operation | Target |
|------------|---------|
| L2 Book Update | < 50 ns |
| Order Match | < 100 ns |
| Event Publish | < 30 ns |
| Participant Notification | < 50 ns |
| Tick-to-Trade Pipeline | < 1 μs |

Once benchmarked, this section will include:

- p50 / p99 / p99.9 latency
- Hardware specifications
- Compiler flags
- Benchmark methodology
- Commit hash for reproducibility

---

# Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release

cmake --build build -j$(nproc)
```

---

# Tests

```bash
ctest --test-dir build --output-on-failure
```

---

# ThreadSanitizer

```bash
cmake -B build-tsan \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="-fsanitize=thread"

cmake --build build-tsan

./build-tsan/tests/queue_stress_test
```

---

# Benchmarks

```bash
./build/bench/book_benchmark

./build/bench/queue_benchmark
```

---

# Development Roadmap

## Phase 1

- Reverse Vector Order Book
- Matching Engine
- Unit Tests

## Phase 2

- Lock-Free Event Queue
- Market Replay
- Exchange Simulator

## Phase 3

- Python bindings (pybind11)
- Participant API
- Avellaneda-MM integration

## Phase 4

- Historical replay
- Synthetic market generator
- Latency benchmarking

## Phase 5

- Multi-participant simulation
- gRPC interface
- Advanced exchange analytics

---

# Related Projects

| Repository | Purpose |
|------------|---------|
| **liquid-book** | Low-latency exchange simulator & matching engine |
| **avellaneda-mm** | Inventory-aware market-making strategy consuming Liquid Book |

Together, these repositories form a modular algorithmic trading research platform:

```text
            Exchange (C++)
                 ▲
                 │
      Participant API
                 │
                 ▼
      Market-Making Strategy (Python)
```

---

# Build Log / Progress

See `docs/description.md` for the phase-by-phase implementation plan and current status.

---

# License

MIT — see `LICENSE`.