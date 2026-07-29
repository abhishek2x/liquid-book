#pragma once

// order_book.hpp — Reverse-vector L2 order book declaration.
//
// Data structure choice (defend on whiteboard):
//   std::vector<PriceLevel> over std::map for cache locality.  map nodes are scattered on the heap
//   (red-black tree), causing a cache miss per level traversal.  A vector keeps all levels
//   contiguous in memory — the common case (updates near the top of book) only touches the last
//   few cache lines.  The tradeoff: O(n) insert/delete for a level far from the best price, but
//   that is rare in practice relative to top-of-book updates.
//
// Layout:
//   bid_levels_: descending price order.  best bid = bid_levels_.back() → O(1) access.
//   ask_levels_: ascending  price order.  best ask = ask_levels_.front() → O(1) access.
//
//   The "reverse vector" trick for bids means that new levels near the current best price can be
//   pushed/popped from the back cheaply, avoiding element shifting for the hot path.

#include "book/price_level.hpp"

#include <optional>
#include <string_view>
#include <vector>

namespace liquidbook {

/// Which side of the book a level belongs to.
enum class Side : uint8_t {
    Bid,
    Ask,
};

/// Parses "BID"/"ASK" (case-insensitive).  Throws std::invalid_argument on unknown input.
Side side_from_string(std::string_view s);

/// L2 order book.  Single-threaded in Phase 1 — no synchronisation primitives.
class OrderBook {
public:
    OrderBook() = default;

    // ── Mutating API ─────────────────────────────────────────────────────────

    /// Apply an L2 update.  qty == 0 removes the level; otherwise insert or update.
    /// Crossing orders (best_bid >= best_ask) are not matched here — that is the
    /// sim exchange's job.  The book simply records the state it is told about.
    void apply_update(Side side, double price, double qty);

    // ── Read API ─────────────────────────────────────────────────────────────

    /// Best bid price, or std::nullopt if the bid side is empty.
    [[nodiscard]] std::optional<double> best_bid() const;

    /// Best ask price, or std::nullopt if the ask side is empty.
    [[nodiscard]] std::optional<double> best_ask() const;

    /// Best bid level (price + qty), or std::nullopt if empty.
    [[nodiscard]] std::optional<PriceLevel> best_bid_level() const;

    /// Best ask level (price + qty), or std::nullopt if empty.
    [[nodiscard]] std::optional<PriceLevel> best_ask_level() const;

    /// All bid levels, best (highest) price last.
    [[nodiscard]] const std::vector<PriceLevel>& bid_levels() const noexcept { return bid_levels_; }

    /// All ask levels, best (lowest) price first.
    [[nodiscard]] const std::vector<PriceLevel>& ask_levels() const noexcept { return ask_levels_; }

    /// Number of distinct price levels on the bid side.
    [[nodiscard]] std::size_t bid_depth() const noexcept { return bid_levels_.size(); }

    /// Number of distinct price levels on the ask side.
    [[nodiscard]] std::size_t ask_depth() const noexcept { return ask_levels_.size(); }

    /// True if the book has no levels on either side.
    [[nodiscard]] bool empty() const noexcept { return bid_levels_.empty() && ask_levels_.empty(); }

private:
    // bid_levels_ sorted descending (best bid = .back()).
    std::vector<PriceLevel> bid_levels_;

    // ask_levels_ sorted ascending (best ask = .front()).
    std::vector<PriceLevel> ask_levels_;

    // Internal helpers
    void apply_bid_update(double price, double qty);
    void apply_ask_update(double price, double qty);
};

}  // namespace liquidbook
