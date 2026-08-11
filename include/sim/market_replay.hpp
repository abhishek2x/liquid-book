#pragma once

#include "book/order_book.hpp"
#include "sim/sim_exchange.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace liquidbook
{

  struct ReplayEvent
  {
    uint64_t timestamp_ns{0};
    Side side{Side::Bid};
    double price{0.0};
    double qty{0.0};
    std::string action{};
  };

  class MarketReplay
  {
  public:
    explicit MarketReplay(std::filesystem::path csv_path);

    [[nodiscard]] std::vector<Trade> run(OrderBook &book, SimExchange &sim);

  private:
    std::filesystem::path csv_path_;
  };

} // namespace liquidbook
