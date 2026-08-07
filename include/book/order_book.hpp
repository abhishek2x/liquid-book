#pragma once

#include "book/price_level.hpp"

#include <optional>
#include <string_view>
#include <vector>

namespace liquidbook
{

  enum class Side : uint8_t
  {
    Bid,
    Ask,
  };

  Side side_from_string(std::string_view s);

  class OrderBook
  {
  public:
    OrderBook() = default;

    void apply_update(Side side, double price, double qty);

    [[nodiscard]] std::optional<double> best_bid() const;
    [[nodiscard]] std::optional<double> best_ask() const;

    [[nodiscard]] std::optional<PriceLevel> best_bid_level() const;
    [[nodiscard]] std::optional<PriceLevel> best_ask_level() const;

    [[nodiscard]] const std::vector<PriceLevel> &bid_levels() const noexcept
    {
      return bid_levels_;
    }
    [[nodiscard]] const std::vector<PriceLevel> &ask_levels() const noexcept
    {
      return ask_levels_;
    }

    [[nodiscard]] std::size_t bid_depth() const noexcept
    {
      return bid_levels_.size();
    }
    [[nodiscard]] std::size_t ask_depth() const noexcept
    {
      return ask_levels_.size();
    }

    [[nodiscard]] bool empty() const noexcept
    {
      return bid_levels_.empty() && ask_levels_.empty();
    }

  private:
    std::vector<PriceLevel> bid_levels_;
    std::vector<PriceLevel> ask_levels_;

    void apply_bid_update(double price, double qty);
    void apply_ask_update(double price, double qty);
  };

} // namespace liquidbook
