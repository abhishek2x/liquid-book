# L2 Order Book — Reverse-Vector Layout

This document explains the data structure choice, internal sorting layouts, and insertion/deletion heuristics used in the `liquid-book` order book implementation.

## Data Structure Choice: Vector vs. Map

Traditionally, order books are modeled using tree-based maps (like `std::map` or `std::unordered_map` with linked lists) because they offer stable pointer references and theoretical $O(\log N)$ or $O(1)$ operations. However, tree nodes are scattered across the heap, leading to cache misses during traversals.

In quantitative trading, most order book updates occur at or near the top of the book (the spread). To optimize cache locality, `liquid-book` implements the L2 book using contiguous memory arrays:
- **`std::vector<PriceLevel>`** represents each side of the book.
- Updates near the best bid or ask touch contiguous cache lines, drastically reducing traversal latency.
- Insertion/deletion of levels far from the spread incurs an $O(N)$ memory-shift overhead, but this is rare in practice compared to updates at the top of the book.

## Internal Layout & Sorting

To optimize operations at the best bid and best ask:
- **Bids (`bid_levels_`)**: Sorted in descending order. The best (highest) bid price is positioned at the back of the vector (`bid_levels_.back()`).
  - **The Reverse Vector Trick**: New bid levels near the best price can be pushed or popped from the back of the vector in $O(1)$ time, avoiding memory shifting for the hot path.
- **Asks (`ask_levels_`)**: Sorted in ascending order. The best (lowest) ask price is positioned at the front of the vector (`ask_levels_.front()`).

## Match Logic Division of Labor

The order book strictly records the state it is told about. It does not perform order matching or matching of crossing bids and asks. The detection and handling of crossing orders is delegated entirely to the simulation exchange layer.
