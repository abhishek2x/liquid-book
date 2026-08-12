#pragma once

#include "book/order_book.hpp"
#include "sim/sim_exchange.hpp"

#include <filesystem>
#include <string>
#include <vector>
#include <functional>

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

    // Stream events: for each produced Trade, `on_trade` is invoked. If `on_trade`
    // returns false, the replay stops early and `run_stream` returns false.
    [[nodiscard]] bool run_stream(OrderBook &book, SimExchange &sim,
                                  const std::function<bool(const Trade &)> &on_trade);

  private:
    std::filesystem::path csv_path_;
  };

} // namespace liquidbook
