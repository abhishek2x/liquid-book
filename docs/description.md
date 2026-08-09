# liquid-book — Development Plan & Reference

This is the working doc to refer back to when starting/resuming dev work. Update the **Status** column as phases land — don't mark anything ✅ until it's actually merged with passing tests/benchmarks committed.

---

## Ground rules (read before writing code)

1. **Commit incrementally, in the order below.** No squashed "initial commit with everything." A reviewer or interviewer clicking through the history should see the design come together honestly.
2. **Never write a number into the README that wasn't produced by code that's actually committed.** If a target isn't measured yet, it stays labeled "Target," not "Result."
3. **Tests before benchmarks, always.** A fast wrong answer is worse than a slow right one.
4. **Every "why" decision gets one sentence in this doc**, next to the phase it belongs to — future-you (or an interviewer) will ask.

---

## Phase 1 — Order Book (no concurrency yet)

**Status: ✅ Done**

Files:

```
include/book/price_level.hpp
include/book/order_book.hpp
src/book/order_book.cpp
tests/book_test.cpp
```

**Design:**

- `PriceLevel`: `{price, aggregate_qty, order_count}`. No per-order tracking in v1 — that's a v2 extension if time allows (needed for true price-time priority; v1 is price-priority only, aggregated at each level).
- `OrderBook`: two `std::vector<PriceLevel>`:
  - Ask side: ascending price, best ask = `.front()`.
  - Bid side: **descending** price, best bid = `.back()` (the "reverse vector" trick — new/recent price levels tend to cluster near the current best, so inserts near the back are cheap).
- Insert/update: walk from the relevant end, shift elements only when a genuinely new price level is inserted mid-book (rare relative to updates to existing levels).
- Delete: remove level when `aggregate_qty` hits zero.

**Why this data structure over `std::map`:** `std::map` gives O(log n) with pointer-chasing and poor cache locality (red-black tree nodes scattered on the heap). A vector keeps levels contiguous — better cache behavior for the common case (updates near the top of book), at the cost of O(n) worst-case insert for a level far from the best price. This is a legitimate tradeoff to be ready to defend, not a strictly-better claim.

**Testing plan:**

- Reference implementation: a naive `std::map<price, qty>` book, built alongside as the ground truth.
- Property test: generate N random order sequences (inserts/updates/cancels), run through both implementations, assert identical best-bid/ask and full depth after every operation.
- Edge cases explicitly tested: empty book, single order, crossing orders (should not happen at book level — that's the matching engine's job — but book should not silently corrupt state if it does), level fully drained to zero.

**Definition of done for this phase:** `ctest` green, property test run with N ≥ 10,000 random sequences, committed with a descriptive commit message. This commit alone is resume-worthy — don't wait for Phase 2 to write it up.

---

## Phase 2 — Lock-Free SPMC FastQueue

**Status: ✅ Done**

Files:

```
include/queue/fast_queue.hpp
tests/queue_test.cpp
tests/queue_stress_test.cpp   # TSan target
```

**Design:**

- Fixed-capacity ring buffer, capacity is a power of 2 → use `& (capacity - 1)` instead of `% capacity` for the index wrap (modulo is surprisingly expensive in a hot loop; bitmask is one instruction).
- Single producer. N consumers, **each with its own read cursor** — this is what makes it SPMC-safe: no consumer can starve or corrupt another's read position.
- Cursor layout: `alignas(64) std::atomic<uint64_t>` per cursor. The `alignas(64)` matters because two atomics sharing a 64-byte cache line cause **false sharing** — one core's write to its cursor invalidates the cache line for every other core reading a _different_ cursor on the same line, even though they're logically independent. Padding each cursor to its own cache line eliminates this.

**Memory ordering (the part interviewers actually probe):**

- Producer writes the slot data first (relaxed store is fine — it's plain data, not yet visible to consumers), **then** publishes by doing a `release` store to the write index.
- Consumer does an `acquire` load of the write index, **then** reads the slot data.
- Why not `seq_cst` everywhere: `seq_cst` forces a total global order across _all_ atomics in the program, which costs a full memory fence on most architectures. `acquire`/`release` only guarantees ordering _between this producer and this consumer_ on _this_ variable — which is exactly the guarantee needed (the data write happens-before the index write, which happens-before the index read, which happens-before the data read). No stronger guarantee is required, so paying for one is wasted latency.
- Be ready to explain this on a whiteboard without the README open.

**Testing plan:**

- Correctness (single-threaded): push N items, pop N items, verify order and values.
- Stress test: 1 producer + K consumer threads (K = 2, 4, 8), each consumer counts/checksums what it reads, run under `-fsanitize=thread`. Zero TSan warnings is the bar — not "fewer" warnings, zero.
- Run the stress test with a high iteration count (millions of ops) — races are often intermittent and won't show up in a quick run.

**Definition of done:** TSan-clean stress test committed, with the actual TSan log output saved in `tests/tsan_output.txt` or referenced in the commit message so it's verifiable, not just claimed.

---

## Phase 3 — Sim Exchange & Market Replay

**Status: 🔧 In progress**

Files:

```
include/sim/market_replay.hpp
include/sim/sim_exchange.hpp
src/sim/market_replay.cpp
src/sim/sim_exchange.cpp
data/sample_l2.csv
```

**Design:**

- Replay: parse deterministic L2 CSV rows, apply them to the book in timestamp order, and emit a trade record whenever a crossing occurs.
- Sim exchange: on crossing, match against the best resting price and consume the resting quantity in a simple aggregate model; this is the v1 implementation before per-order queue arrays are introduced.
- Why this is the right cut: the book remains cache-friendly and the simulator handles crossing semantics without overloading the book data structure with per-order tracking.

**Definition of done:** A full run of `data/sample_l2.csv` through replay → book → sim exchange completes without crashing/asserting, produces a trade log, and the trade log is spot-checked by hand against a few known points in the sample data.

---

## Phase 4 — Risk Guards

**Status: ⏳ Not started**

Files:

```
include/risk/limits.hpp
```

Simple, deliberately: position limit check + drawdown check, both O(1), both able to halt the engine (kill switch) on breach. This phase is short on purpose — it's an integration point, not where the engineering depth lives.

---

## Phase 5 — Benchmarks

**Status: ⏳ Not started**

Files:

```
bench/book_benchmark.cpp
bench/queue_benchmark.cpp
```

**Method:**

- Google Benchmark, **Release build only** (`-DCMAKE_BUILD_TYPE=Release`) — a Debug-build number is meaningless and will be spotted immediately by anyone who's done this before.
- Pin the benchmark process to an isolated core (`taskset -c N ./book_benchmark` or `sched_setaffinity` in-code) to reduce scheduler noise.
- Report **p50/p99/p99.9**, not mean. Mean hides tail latency, which is the number that actually matters in this domain — and reporting percentiles instead of an average is itself a signal of understanding the domain, worth calling out explicitly in the README once done.
- Record: CPU model, compiler + version, flags used, and the commit hash the numbers came from. Numbers without this context aren't reproducible and won't be trusted.

**Definition of done:** `bench/` output committed (as a file or pasted into the README under "Benchmark Results," replacing "Performance Targets"), with the context above included.

---

## Explicitly out of scope for this repo

- Any trading strategy logic (OFI, micro-price, Avellaneda-Stoikov quoting) — that's [`avellaneda-mm`](https://github.com/abhishek2x/avellaneda-mm), which is written in Python and depends on this repo as a library.
- ML/regime detection, pybind11 bridge — also lives in `avellaneda-mm` (where `pybind11` wraps `liquid-book`'s C++ library for zero-IPC in-memory Python calls), and only if time allows there.
- Real exchange connectivity of any kind. This is a simulator, not a trading system, and should never be described as one.

## Next session checklist

When resuming work, before writing any code:

1. Re-read the "Status" column above — resume at the first ⏳, don't skip ahead.
2. Run `ctest` on whatever's already there to confirm nothing regressed.
3. Update this file's Status column and commit the doc update _with_ the code change, same commit.
