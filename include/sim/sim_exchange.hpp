#pragma once

#include "book/order_book.hpp"

#include <cstdint>
#include <optional>

namespace liquidbook
{

  struct Trade
  {
    uint64_t timestamp_ns{0};
    Side aggressor_side{Side::Bid};
    double price{0.0};
    double qty{0.0};
  };

  class SimExchange
  {
  public:
    SimExchange() = default;

    [[nodiscard]] bool is_crossing(const OrderBook &book) const noexcept;

    [[nodiscard]] std::optional<Trade> match_crossing(OrderBook &book,
                                                    Side aggressor_side,
                                                    uint64_t timestamp_ns = 0,
                                                    double aggressor_qty = 0.0);
  };

} // namespace liquidbook
