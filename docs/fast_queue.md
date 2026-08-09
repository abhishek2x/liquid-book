# FastQueue — Bounded Lock-Free SPMC Ring Buffer

This document details the design, false sharing prevention mechanisms, and memory ordering model used in the Single Producer Multi Consumer (SPMC) ring buffer implementation.

## The Problem It Solves

In an order book system, one thread typically produces a stream of events — trades, book updates, order acks — that several independent downstream consumers need to see _in full and in order_: a market-data publisher, a risk snapshotter, a logger, a strategy engine. This is a **broadcast** pattern (every consumer sees every item), not a work-queue pattern (where each item goes to exactly one consumer).

The naive way to do this is a `std::queue<T>` guarded by a `std::mutex`, with either one queue per consumer or one shared queue and a condition variable per reader. Both run into the same core problems under load:

- **Lock contention**: every push and every pop takes the mutex. With multiple consumers polling, the producer is competing with N threads for the same lock on the hot path, which is exactly where you can least afford to block in a matching engine.
- **Unpredictable latency (tail latency)**: a thread holding the lock can be descheduled by the OS mid-critical-section. Every other thread then stalls until it's rescheduled — this is the classic "lock convoy" problem, and it shows up as latency spikes that are much worse than the average.
- **Dynamic allocation**: `std::queue` (backed by `std::deque` by default) allocates nodes as it grows. Allocation is slow, non-deterministic in timing, and can trigger page faults — all things you want to eliminate in a latency-sensitive path.
- **Cache traffic from shared state**: even a lock-free `std::atomic`-based design can silently reintroduce contention if unrelated variables (e.g., two consumers' cursors) happen to share a 64-byte cache line — this is **false sharing**, and it doesn't show up in the code, only in profiling.

`FastQueue` is built specifically to remove all four of these costs for the SPMC broadcast case.

## Design Overview

`FastQueue` is a bounded ring buffer that enables a single producer to publish data events to multiple independent consumers. Each consumer maintains its own read cursor, meaning every consumer receives every item pushed to the queue.

- **Fixed Capacity**: The queue is pre-allocated inline using a `std::array` (zero dynamic allocations). The capacity must be a power of two to optimize index wrapping via bitwise AND (`index & (Capacity - 1)`) rather than costly modulo division.
- **Single Producer**: Only one thread calls `try_push`.
- **Multiple Consumers**: Up to `MaxConsumers` threads can concurrently call `try_pop` using their assigned `consumer_id`.

### Why This Beats the Alternatives

| Approach                        | Allocation                                               | Synchronization                                                              | Broadcast to N readers                                                               | Tail latency                                                         |
| ------------------------------- | -------------------------------------------------------- | ---------------------------------------------------------------------------- | ------------------------------------------------------------------------------------ | -------------------------------------------------------------------- |
| `std::queue` + `std::mutex`     | Dynamic, per-node                                        | Blocking lock, held by producer _and_ consumers                              | Needs N separate queues or a shared queue with refcounting                           | Poor — lock convoys, OS scheduler at the mercy of who holds the lock |
| `boost::lockfree::queue` (MPMC) | Usually dynamic (unless a fixed-size pool is configured) | Lock-free CAS loops                                                          | Not designed for broadcast — an item popped by one consumer is _gone_ for the others | Better than mutex, but CAS retries under contention still add jitter |
| `FastQueue` (SPMC ring buffer)  | None — `std::array`, fixed at compile time               | Lock-free, single `release`/`acquire` pair per operation, no CAS loop needed | Native — every consumer has its own cursor into the _same_ backing storage           | Best — bounded, no allocation, no blocking, no false sharing         |

The key structural insight is that because there is only **one producer**, the producer never needs a compare-and-swap loop to update `write_pos_` — it's the only writer, so a plain relaxed load followed by a release store is enough. Correctness for the multi-consumer side is handled not by locking, but by giving each consumer a private, cache-line-isolated cursor into shared, immutable-once-published storage. That's what makes it faster than a general-purpose MPMC lock-free queue for this specific access pattern: it doesn't pay for the CAS retry loop that MPMC needs to arbitrate between multiple producers, because that problem simply doesn't exist here.

## False Sharing Prevention

On modern multi-core CPUs, caching is managed in lines (typically 64 bytes). When two cores write to different variables residing on the same cache line, they invalidate the line for each other, forcing expensive cache reloads. This is known as **false sharing**.

To prevent false sharing:

- The single producer's write cursor (`write_pos_`) is aligned to 64 bytes:
  ```cpp
  alignas(64) std::atomic<uint64_t> write_pos_{0};
  ```
- Each consumer's read cursor is aligned to 64 bytes inside a wrapper struct (`AlignedCursor`):
  ```cpp
  struct alignas(64) AlignedCursor {
      std::atomic<uint64_t> value{0};
  };
  ```
  This ensures each cursor sits on its own cache line, eliminating memory thrashing when cursors are updated in parallel.

## Memory Ordering

To minimize synchronization overhead, `FastQueue` utilizes targeted memory orders rather than sequentially consistent fences (`std::memory_order_seq_cst`).

### Producer (`try_push`)

1. **Load `write_pos_`** (`std::memory_order_relaxed`): Safe since only the producer thread updates this cursor.
2. **Check Capacity** (`std::memory_order_acquire`): Loads all active consumers' read cursors to verify the slowest consumer is not overtaken. Using `acquire` guarantees we see the consumer's increment.
3. **Write Data** (relaxed): Copy/assignment to `storage_` is safe because the slot is guaranteed to not be in use by any consumer.
4. **Publish Cursor** (`std::memory_order_release`): Increments `write_pos_` using `release` ordering. This creates a happens-before relationship, guaranteeing that the written data is visible to any consumer before they read the advanced write cursor.

### Consumer (`try_pop`)

1. **Load Own `read_pos_`** (`std::memory_order_relaxed`): Safe since only this specific consumer thread writes to this cursor.
2. **Load `write_pos_`** (`std::memory_order_acquire`): Synchronizes with the producer's release store, ensuring all data written to the slot is visible.
3. **Read Data**: Safe to read the element since the write cursor has been published and acquire-loaded.
4. **Advance own cursor** (`std::memory_order_release`): Increments the consumer's read cursor using `release` ordering. This ensures the slot read is complete before the producer can see the slot is free.

## Worked Example

Assume a queue with `Capacity = 4` (so the wrap mask is `Capacity - 1 = 3`) and two consumers: a **Logger** (`consumer_id = 0`) and a **Risk Engine** (`consumer_id = 1`).

Initial state: `write_pos_ = 0`, `read_pos_[Logger] = 0`, `read_pos_[Risk] = 0`. All four slots are empty.

**Step 1 — Producer pushes 3 trade events (`T1`, `T2`, `T3`)**

For each push, `try_push`:

1. Loads `write_pos_` relaxed (0, then 1, then 2).
2. Checks capacity: it reads the minimum of all consumers' `read_pos_` (currently 0), confirms `write_pos_ - min(read_pos_) < Capacity`, so there's room.
3. Writes the event into `storage_[write_pos_ & 3]` → slots `0`, `1`, `2`.
4. Releases the new `write_pos_` → after three pushes, `write_pos_ = 3`.

Neither consumer has read anything yet, but nothing has blocked — the producer never waited on either consumer.

**Step 2 — Logger consumes at its own pace**

Logger calls `try_pop`:

1. Loads its own `read_pos_[Logger]` relaxed → `0`.
2. Loads `write_pos_` acquire → sees `3`, so 3 items are available (`3 - 0 > 0`).
3. Reads `storage_[0 & 3]` → gets `T1`. Because the load of `write_pos_` was `acquire` and paired with the producer's `release` store, the Logger is guaranteed to see the fully-written `T1`, not a half-written struct.
4. Releases its own `read_pos_[Logger]` → now `1`.

Logger can keep calling `try_pop` to drain `T2` and `T3` independently, or fall behind — it doesn't affect the Risk Engine.

**Step 3 — Risk Engine hasn't read anything yet, producer pushes a 4th event (`T4`)**

`write_pos_` is currently `3`. Capacity check for the new push:

- `min(read_pos_[Logger], read_pos_[Risk])` = `min(1, 0)` = `0` (Risk Engine is the slow reader).
- `write_pos_ - min = 3 - 0 = 3`, which is `< Capacity (4)`, so there's still one free slot.
- Producer writes `T4` into `storage_[3 & 3]` = `storage_[3]`, then releases `write_pos_ = 4`.

**Step 4 — Producer tries to push a 5th event (`T5`) before Risk Engine reads anything**

- `min(read_pos_)` is still `0` (Risk Engine's cursor).
- `write_pos_ - min = 4 - 0 = 4`, which is **not** `< Capacity (4)` — the ring is full from the slowest consumer's point of view.
- `try_push` returns `false` (backpressure). The producer does not overwrite `storage_[0]`, because that slot's contents (`T1`) haven't been consumed by the Risk Engine yet — overwriting it would silently corrupt what Risk Engine is about to read.

This is the crux of the design: the ring buffer's usable capacity is always bounded by the **slowest** consumer. `FastQueue` protects correctness by refusing to advance past a consumer that hasn't caught up, rather than by locking anyone out while it decides.

**Step 5 — Risk Engine finally reads**

Risk Engine calls `try_pop` four times, draining `T1` → `T2` → `T3` → `T4`, advancing `read_pos_[Risk]` from `0` to `4`. Now `min(read_pos_) = min(1..3, 4) `depends on Logger's progress — once Logger also reaches `4`, the producer's next `try_push` for `T5` succeeds because `write_pos_ - min(read_pos_) = 4 - 4 = 0 < 4`.

**Takeaways from the example**

- Each consumer reads every item; nothing is "consumed away" from another consumer, unlike a work-stealing or MPMC queue.
- The producer never blocks on a mutex — it either succeeds immediately or returns `false` immediately (`try_push`), which the caller can use as a real backpressure signal instead of stalling the hot path.
- Correctness under concurrent access comes entirely from the `acquire`/`release` pairing on `write_pos_` and each `read_pos_`, not from a lock — the "happens-before" relationship guarantees a consumer never observes a slot's new data before the producer's write is fully complete, and the producer never overwrites a slot before every consumer's read of it is complete.
