#include "sim/sim_exchange.hpp"

#include <algorithm>
#include <optional>

namespace liquidbook
{

  bool SimExchange::is_crossing(const OrderBook &book) const noexcept
  {
    const auto best_bid = book.best_bid();
    const auto best_ask = book.best_ask();
    return best_bid.has_value() && best_ask.has_value() && (*best_bid >= *best_ask);
  }

  std::optional<Trade> SimExchange::match_crossing(OrderBook &book,
                                                 Side aggressor_side,
                                                 uint64_t timestamp_ns,
                                                 double aggressor_qty)
  {
    if (!is_crossing(book))
    {
      return std::nullopt;
    }

    if (aggressor_side == Side::Bid)
    {
      const auto ask_level = book.best_ask_level();
      const auto bid_level = book.best_bid_level();
      if (!ask_level || !bid_level)
      {
        return std::nullopt;
      }

      const auto trade_qty = std::min(ask_level->aggregate_qty,
                                      aggressor_qty > 0.0 ? aggressor_qty : bid_level->aggregate_qty);
      Trade trade{timestamp_ns, Side::Bid, ask_level->price, trade_qty};
      const auto remaining_ask = ask_level->aggregate_qty - trade_qty;
      book.apply_update(Side::Ask, ask_level->price, remaining_ask > 0.0 ? remaining_ask : 0.0);
      const auto remaining_bid = bid_level->aggregate_qty - trade_qty;
      if (remaining_bid > 0.0)
      {
        book.apply_update(Side::Bid, bid_level->price, remaining_bid);
      }
      else
      {
        book.apply_update(Side::Bid, bid_level->price, 0.0);
      }
      return trade;
    }

    const auto bid_level = book.best_bid_level();
    const auto ask_level = book.best_ask_level();
    if (!bid_level || !ask_level)
    {
      return std::nullopt;
    }

    const auto trade_qty = std::min(bid_level->aggregate_qty,
                                    aggressor_qty > 0.0 ? aggressor_qty : ask_level->aggregate_qty);
    Trade trade{timestamp_ns, Side::Ask, bid_level->price, trade_qty};
    const auto remaining_bid = bid_level->aggregate_qty - trade_qty;
    if (remaining_bid > 0.0)
    {
      book.apply_update(Side::Bid, bid_level->price, remaining_bid);
    }
    else
    {
      book.apply_update(Side::Bid, bid_level->price, 0.0);
    }
    const auto remaining_ask = ask_level->aggregate_qty - trade_qty;
    if (remaining_ask > 0.0)
    {
      book.apply_update(Side::Ask, ask_level->price, remaining_ask);
    }
    else
    {
      book.apply_update(Side::Ask, ask_level->price, 0.0);
    }
    return trade;
  }

} // namespace liquidbook
