#include "book/order_book.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>

namespace liquidbook {

Side side_from_string(std::string_view s) {
    std::string upper;
    upper.reserve(s.size());
    for (char c : s) {
        upper += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    if (upper == "BID") return Side::Bid;
    if (upper == "ASK") return Side::Ask;
    throw std::invalid_argument("Unknown side string: " + std::string(s));
}

void OrderBook::apply_bid_update(double price, double qty) {
    for (auto it = bid_levels_.rbegin(); it != bid_levels_.rend(); ++it) {
        if (it->price == price) {
            if (qty == 0.0) {
                bid_levels_.erase(std::next(it).base());
            } else {
                it->aggregate_qty = qty;
            }
            return;
        }
        if (it->price < price) {
            if (qty != 0.0) {
                bid_levels_.insert(it.base(), PriceLevel{price, qty, 1});
            }
            return;
        }
    }

    if (qty != 0.0) {
        bid_levels_.insert(bid_levels_.begin(), PriceLevel{price, qty, 1});
    }
}

void OrderBook::apply_ask_update(double price, double qty) {
    for (auto it = ask_levels_.begin(); it != ask_levels_.end(); ++it) {
        if (it->price == price) {
            if (qty == 0.0) {
                ask_levels_.erase(it);
            } else {
                it->aggregate_qty = qty;
            }
            return;
        }
        if (it->price > price) {
            if (qty != 0.0) {
                ask_levels_.insert(it, PriceLevel{price, qty, 1});
            }
            return;
        }
    }

    if (qty != 0.0) {
        ask_levels_.push_back(PriceLevel{price, qty, 1});
    }
}

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

} // namespace liquidbook
