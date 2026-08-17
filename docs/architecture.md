# System Architecture & Technical Specifications

This document outlines the complete architectural design, component interactions, and low-latency design decisions of `liquid-book`.

## Overview

`liquid-book` is designed as a standalone, strategy-agnostic exchange simulator. Unlike minimal matching engine tutorials that combine order book state and strategy logic in a single monolith, `liquid-book` maintains clear boundaries between:
- Order Book Depth (L2 state representation)
- Matching Engine & Simulation Exchange (crossing & execution semantics)
- Event Bus / Dispatcher (lock-free SPMC broadcast queue)
- Participant API & Strategies (external clients, e.g. Python via pybind11)

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

---

## Component Breakdown

### 1. Reverse Vector Order Book (`include/book/order_book.hpp`)
- **Structure**: Uses two contiguous `std::vector<PriceLevel>` containers for bids and asks.
- **Reverse Vector Layout**: Bids are stored in descending price order with the best bid at `.back()`, enabling $O(1)$ push/pop updates at the spread.
- **Cache Locality**: Avoids red-black tree node allocations (`std::map`), keeping hot price level depth contiguous in cache memory.

### 2. Lock-Free FastQueue (`include/queue/fast_queue.hpp`)
- **SPMC Ring Buffer**: Single Producer, Multiple Consumer circular buffer template with compile-time power-of-two capacity.
- **False-Sharing Prevention**: Cursor structures padded to 64-byte boundaries (`alignas(64)`).
- **Acquire/Release Semantics**: Fine-grained memory ordering to eliminate global CPU bus locks (`seq_cst`).

### 3. SimExchange & Market Replay (`include/sim/`)
- **Deterministic Replay**: Reads timestamped CSV market events and updates order book state deterministically.
- **Crossing Matcher**: Resolves spread crossings at resting price priority.

### 4. Exchange Risk Guards (`include/risk/limits.hpp`)
- **Position & Drawdown Limits**: Real-time evaluation of net inventory and drawdown thresholds.
- **Kill Switch Interlock**: Atomic signaling to halt execution immediately upon risk breach.

---

## Concurrent Threading Pipeline

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
