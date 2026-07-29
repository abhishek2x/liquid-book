#pragma once

// price_level.hpp — Aggregate representation of a single price level in the order book.
//
// Design (v1): price-priority only, no per-order tracking.
// Each level stores the total quantity resting at that price and the number of individual
// orders contributing to it.  Per-order (price-time priority) tracking is a v2 extension.

#include <cstdint>

namespace liquidbook {

/// A single price level: the basic unit of an L2 order book.
struct PriceLevel {
    double   price;          ///< Price of this level (exact representation; use double consistently)
    double   aggregate_qty;  ///< Total quantity resting at this price across all orders
    int32_t  order_count;    ///< Number of individual orders at this level (informational in v1)
};

}  // namespace liquidbook
