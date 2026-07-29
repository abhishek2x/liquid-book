// order_book.cpp — Reverse-vector L2 order book implementation.

#include "book/order_book.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>

namespace liquidbook {

// ── Helpers ──────────────────────────────────────────────────────────────────

Side side_from_string(std::string_view s) {
    // Uppercase comparison for "BID"/"ASK"
    std::string upper;
    upper.reserve(s.size());
    for (char c : s) upper += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if (upper == "BID") return Side::Bid;
    if (upper == "ASK") return Side::Ask;
    throw std::invalid_argument("Unknown side string: " + std::string(s));
}

// ── Private helpers ──────────────────────────────────────────────────────────

// Bid side: descending price.  best bid = bid_levels_.back().
// We scan from the back (best price) forward — updates near top-of-book are O(1).
void OrderBook::apply_bid_update(double price, double qty) {
    // Search from the back (highest price = best bid).
    for (auto it = bid_levels_.rbegin(); it != bid_levels_.rend(); ++it) {
        if (it->price == price) {
            // Existing level found.
            if (qty == 0.0) {
                // Erase: convert reverse_iterator → forward iterator for erase().
                bid_levels_.erase(std::next(it).base());
            } else {
                it->aggregate_qty = qty;
            }
            return;
        }
        if (it->price < price) {
            // Insert a new level between *it and the element before it (i.e. at higher price).
            // Convert to forward iterator pointing just after *it.
            if (qty != 0.0) {
                bid_levels_.insert(it.base(), PriceLevel{price, qty, 1});
            }
            // qty == 0 and no existing level → no-op (nothing to delete).
            return;
        }
    }

    // We walked the whole vector without finding a price <= the new price.
    // That means this price is lower than every existing bid, or the bid side is empty.
    // Insert at the front (lowest index = lowest price in descending order).
    if (qty != 0.0) {
        bid_levels_.insert(bid_levels_.begin(), PriceLevel{price, qty, 1});
    }
}

// Ask side: ascending price.  best ask = ask_levels_.front().
// We scan from the front (lowest price = best ask) forward — updates near top-of-book are O(1).
void OrderBook::apply_ask_update(double price, double qty) {
    for (auto it = ask_levels_.begin(); it != ask_levels_.end(); ++it) {
        if (it->price == price) {
            // Existing level found.
            if (qty == 0.0) {
                ask_levels_.erase(it);
            } else {
                it->aggregate_qty = qty;
            }
            return;
        }
        if (it->price > price) {
            // New price is better (lower ask) than this element — insert before it.
            if (qty != 0.0) {
                ask_levels_.insert(it, PriceLevel{price, qty, 1});
            }
            return;
        }
    }

    // New price is higher than every existing ask (or ask side is empty).
    if (qty != 0.0) {
        ask_levels_.push_back(PriceLevel{price, qty, 1});
    }
}

// ── Public API ───────────────────────────────────────────────────────────────

void OrderBook::apply_update(Side side, double price, double qty) {
    if (side == Side::Bid) {
        apply_bid_update(price, qty);
    } else {
        apply_ask_update(price, qty);
    }
}

std::optional<double> OrderBook::best_bid() const {
    if (bid_levels_.empty()) return std::nullopt;
    return bid_levels_.back().price;
}

std::optional<double> OrderBook::best_ask() const {
    if (ask_levels_.empty()) return std::nullopt;
    return ask_levels_.front().price;
}

std::optional<PriceLevel> OrderBook::best_bid_level() const {
    if (bid_levels_.empty()) return std::nullopt;
    return bid_levels_.back();
}

std::optional<PriceLevel> OrderBook::best_ask_level() const {
    if (ask_levels_.empty()) return std::nullopt;
    return ask_levels_.front();
}

}  // namespace liquidbook
