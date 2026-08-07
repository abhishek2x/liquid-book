# FastQueue — Bounded Lock-Free SPMC Ring Buffer

This document details the design, false sharing prevention mechanisms, and memory ordering model used in the Single Producer Multi Consumer (SPMC) ring buffer implementation.

## Design Overview

`FastQueue` is a bounded ring buffer that enables a single producer to publish data events to multiple independent consumers. Each consumer maintains its own read cursor, meaning every consumer receives every item pushed to the queue. 

- **Fixed Capacity**: The queue is pre-allocated inline using a `std::array` (zero dynamic allocations). The capacity must be a power of two to optimize index wrapping via bitwise AND (`index & (Capacity - 1)`) rather than costly modulo division.
- **Single Producer**: Only one thread calls `try_push`.
- **Multiple Consumers**: Up to `MaxConsumers` threads can concurrently call `try_pop` using their assigned `consumer_id`.

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
