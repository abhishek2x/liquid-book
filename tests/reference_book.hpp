#pragma once

#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace liquidbook {

class ReferenceBook {
public:
    ReferenceBook() = default;

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

    [[nodiscard]] std::optional<double> best_bid() const {
        if (bids_.empty()) return std::nullopt;
        return bids_.rbegin()->first;
    }

    [[nodiscard]] std::optional<double> best_ask() const {
        if (asks_.empty()) return std::nullopt;
        return asks_.begin()->first;
    }

    [[nodiscard]] std::optional<double> best_bid_qty() const {
        if (bids_.empty()) return std::nullopt;
        return bids_.rbegin()->second;
    }

    [[nodiscard]] std::optional<double> best_ask_qty() const {
        if (asks_.empty()) return std::nullopt;
        return asks_.begin()->second;
    }

    [[nodiscard]] std::vector<std::pair<double, double>> bid_snapshot() const {
        std::vector<std::pair<double, double>> out;
        out.reserve(bids_.size());
        for (auto it = bids_.rbegin(); it != bids_.rend(); ++it) {
            out.emplace_back(it->first, it->second);
        }
        return out;
    }

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
    std::map<double, double> bids_;
    std::map<double, double> asks_;
};

} // namespace liquidbook
