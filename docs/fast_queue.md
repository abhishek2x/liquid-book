# FastQueue — Bounded SPMC Broadcast Ring Buffer

`FastQueue` is a fixed-capacity ring buffer for a **Single Producer, Multiple Consumers (SPMC)** broadcast pattern.

One producer publishes each event, and every consumer receives every event independently.

```text
Producer
   |
   +--> Consumer 0
   +--> Consumer 1
   +--> Consumer 2
```

This is different from a work queue, where each item is consumed by only one worker.

## Design Assumptions

- Only one thread calls `try_push()`.
- Each consumer has a unique `consumer_id`.
- One consumer ID is used by only one thread.
- Consumer IDs are in the range `0` to `active_consumers - 1`.
- The queue is not copied or moved.
- `reset()` is not called while push/pop operations are active.
- The queue has a fixed capacity known at compile time.
- `Capacity` must be a power of two.

## Main Data Members

```cpp
std::array<T, Capacity> storage_{};
```

This is the circular buffer. `std::array` avoids dynamic allocation and resizing.

```cpp
std::atomic<uint64_t> write_pos_{0};
```

This stores the producer's next logical write position.

```cpp
std::array<AlignedCursor, MaxConsumers> read_pos_{};
```

Each consumer has an independent read cursor. Therefore, every consumer can read every event independently.

`std::array` is used instead of `std::vector` because the maximum number of consumers and the storage capacity are fixed at compile time.

## Ring Indexing

```cpp
static_assert((Capacity & (Capacity - 1)) == 0,
              "Capacity must be a power of 2");
```

The physical array index is calculated using:

```cpp
position & (Capacity - 1)
```

For `Capacity == 8`:

```text
Logical position 0 -> slot 0
Logical position 7 -> slot 7
Logical position 8 -> slot 0
Logical position 9 -> slot 1
```

The logical positions keep increasing while the physical slots are reused.

## `try_push()`

The producer checks every active consumer:

```cpp
if (write_pos - read_pos >= Capacity)
{
    return false;
}
```

Meaning:

> This consumer is at least `Capacity` items behind. Writing now would overwrite an item that it has not read yet.

If any consumer is too slow, `try_push()` returns `false`. It does not wait and does not overwrite unread data.

The producer then:

1. Writes the item into the next slot.
2. Publishes the new `write_pos_`.
3. Returns `true`.

The slowest consumer controls when the producer can reuse a slot.

## `try_pop()`

Each consumer uses its own cursor:

```cpp
read_pos_[consumer_id]
```

The consumer:

1. Reads its current position.
2. Loads the producer's published position.
3. Returns `false` if no item is available.
4. Reads the item.
5. Advances its own cursor.

Every consumer receives every event:

```text
Consumer 0: Event 1, Event 2, Event 3
Consumer 1: Event 1, Event 2, Event 3
Consumer 2: Event 1, Event 2, Event 3
```

The consumer validation should use:

```cpp
if (consumer_id >= active_consumers_)
{
    return false;
}
```

This ensures that the consumer is actually registered for the queue.

The constructor should also validate:

```text
1 <= active_consumers <= MaxConsumers
```

## Memory Ordering

### Producer

```cpp
write_pos_.load(std::memory_order_relaxed);
```

Only the producer modifies `write_pos_`, so it can load its own position using relaxed ordering.

```cpp
read_pos_[i].value.load(std::memory_order_acquire);
```

The producer acquires each consumer's progress before reusing a slot.

```cpp
write_pos_.store(next, std::memory_order_release);
```

The release store publishes the completed item. A consumer that acquire-loads this value can safely read the producer's earlier write.

### Consumer

```cpp
read_pos_[consumer_id].value.load(
    std::memory_order_relaxed);
```

Only that consumer modifies its own cursor.

```cpp
write_pos_.load(std::memory_order_acquire);
```

This ensures that the consumer sees the item after the producer has fully written it.

```cpp
read_pos_[consumer_id].value.store(
    next,
    std::memory_order_release);
```

This tells the producer that the consumer has finished reading the slot.

## False Sharing

Each consumer frequently updates its own cursor. If multiple cursors share a cache line, updates by one CPU can invalidate data used by another CPU.

The wrapper attempts to place each cursor on a separate cache line:

```cpp
struct alignas(64) AlignedCursor
{
    std::atomic<uint64_t> value{0};
};
```

This reduces false sharing. It is a performance optimization, not a correctness requirement.

## Copying and Moving

The queue disables copying and moving:

```cpp
FastQueue(const FastQueue&) = delete;
FastQueue& operator=(const FastQueue&) = delete;

FastQueue(FastQueue&&) = delete;
FastQueue& operator=(FastQueue&&) = delete;
```

These prevent:

- Copy construction.
- Copy assignment.
- Move construction.
- Move assignment.

A queue represents shared synchronization state and should have one stable identity. It should be created once and passed to threads by reference or pointer.

```cpp
FastQueue<int, 8, 2> queue(2);

std::thread consumer(
    [&queue]()
    {
        int value;
        queue.try_pop(0, value);
    });
```

## `reset()`

`reset()` is safe only when no producer or consumer is using the queue.

Recommended lifecycle:

```text
1. Stop the producer.
2. Stop all consumers.
3. Join or wait for those threads.
4. Call reset().
5. Start using the queue again.
```

Do not call `reset()` concurrently with `try_push()` or `try_pop()`. Resetting positions while operations are active can make the queue state inconsistent.

## Lock-Free Meaning

`std::atomic` makes individual operations indivisible and synchronized. However, an atomic type is not automatically guaranteed to be implemented using lock-free CPU instructions.

This can be checked using:

```cpp
write_pos_.is_lock_free();
```

The queue avoids mutexes and blocking waits under its intended usage model:

- One producer.
- One thread per consumer ID.
- No concurrent reset.
- No resizing or destruction while active.

Therefore, this is a specialized SPMC broadcast queue, not a general MPMC queue.
