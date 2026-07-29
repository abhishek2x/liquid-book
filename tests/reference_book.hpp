#pragma once

// reference_book.hpp — Naive std::map-based L2 book used as ground truth for differential testing.
//
// This is intentionally simple: correctness over performance.  It is the oracle against which
// the reverse-vector OrderBook is compared in property tests.
//
// Bids: std::map<double, double> with reverse iteration (highest price = best bid).
// Asks: std::map<double, double> with forward iteration (lowest price = best ask).

#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace liquidbook {

class ReferenceBook {
public:
    ReferenceBook() = default;

    // ── Mutating API ─────────────────────────────────────────────────────────

    void apply_bid_update(double price, double qty) {
        if (qty == 0.0) {
            bids_.erase(price);
        } else {
            bids_[price] = qty;
        }
    }

    void apply_ask_update(double price, double qty) {
        if (qty == 0.0) {
            asks_.erase(price);
        } else {
            asks_[price] = qty;
        }
    }

    // ── Read API ─────────────────────────────────────────────────────────────

    [[nodiscard]] std::optional<double> best_bid() const {
        if (bids_.empty()) return std::nullopt;
        return bids_.rbegin()->first;  // highest key
    }

    [[nodiscard]] std::optional<double> best_ask() const {
        if (asks_.empty()) return std::nullopt;
        return asks_.begin()->first;   // lowest key
    }

    [[nodiscard]] std::optional<double> best_bid_qty() const {
        if (bids_.empty()) return std::nullopt;
        return bids_.rbegin()->second;
    }

    [[nodiscard]] std::optional<double> best_ask_qty() const {
        if (asks_.empty()) return std::nullopt;
        return asks_.begin()->second;
    }

    /// Snapshot of all bid levels as (price, qty) sorted descending (best first).
    [[nodiscard]] std::vector<std::pair<double, double>> bid_snapshot() const {
        std::vector<std::pair<double, double>> out;
        out.reserve(bids_.size());
        for (auto it = bids_.rbegin(); it != bids_.rend(); ++it) {
            out.emplace_back(it->first, it->second);
        }
        return out;
    }

    /// Snapshot of all ask levels as (price, qty) sorted ascending (best first).
    [[nodiscard]] std::vector<std::pair<double, double>> ask_snapshot() const {
        std::vector<std::pair<double, double>> out;
        out.reserve(asks_.size());
        for (const auto& [p, q] : asks_) {
            out.emplace_back(p, q);
        }
        return out;
    }

    [[nodiscard]] std::size_t bid_depth() const noexcept { return bids_.size(); }
    [[nodiscard]] std::size_t ask_depth() const noexcept { return asks_.size(); }
    [[nodiscard]] bool empty() const noexcept { return bids_.empty() && asks_.empty(); }

private:
    std::map<double, double> bids_;  // key = price, value = aggregate qty
    std::map<double, double> asks_;
};

}  // namespace liquidbook
