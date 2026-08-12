#include "sim/market_replay.hpp"

#include <cctype>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace liquidbook
{
  namespace
  {
    std::string trim(std::string value)
    {
      const auto first = value.find_first_not_of(" \t\r\n");
      if (first == std::string::npos)
      {
        return {};
      }

      const auto last = value.find_last_not_of(" \t\r\n");
      return value.substr(first, last - first + 1);
    }

    std::string upper(std::string value)
    {
      for (char &ch : value)
      {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
      }
      return value;
    }

    std::optional<Side> parse_side(std::string value)
    {
      value = upper(trim(std::move(value)));
      if (value == "BID")
      {
        return Side::Bid;
      }
      if (value == "ASK")
      {
        return Side::Ask;
      }
      return std::nullopt;
    }

    std::optional<ReplayEvent> parse_event_line(const std::string &line)
    {
      std::stringstream stream(line);
      std::string field;
      std::vector<std::string> cols;
      while (std::getline(stream, field, ','))
      {
        cols.push_back(trim(field));
      }

      if (cols.size() != 5)
      {
        return std::nullopt;
      }

      const auto side = parse_side(cols[1]);
      if (!side)
      {
        return std::nullopt;
      }

      ReplayEvent event{};
      event.timestamp_ns = std::stoull(cols[0]);
      event.side = *side;
      event.price = std::stod(cols[2]);
      event.qty = std::stod(cols[3]);
      event.action = upper(cols[4]);
      return event;
    }
  } // namespace

  MarketReplay::MarketReplay(std::filesystem::path csv_path) : csv_path_(std::move(csv_path)) {}

  std::vector<Trade> MarketReplay::run(OrderBook &book, SimExchange &sim)
  {
    std::ifstream input(csv_path_);
    if (!input.is_open())
    {
      throw std::runtime_error("Unable to open replay CSV: " + csv_path_.string());
    }

    std::string line;
    if (!std::getline(input, line))
    {
      return {};
    }

    std::vector<Trade> trades;
    while (std::getline(input, line))
    {
      const auto trimmed = trim(line);
      if (trimmed.empty())
      {
        continue;
      }

      const auto event = parse_event_line(trimmed);
      if (!event)
      {
        throw std::runtime_error("Malformed replay event: " + trimmed);
      }

      const auto action = upper(event->action);
      const double effective_qty = (action == "DELETE" || event->qty == 0.0) ? 0.0 : event->qty;
      book.apply_update(event->side, event->price, effective_qty);

      if (book.best_bid().has_value() && book.best_ask().has_value() &&
          *book.best_bid() >= *book.best_ask())
      {
        const auto aggressor_side = (event->side == Side::Bid) ? Side::Ask : Side::Bid;
        if (const auto trade = sim.match_crossing(book, aggressor_side, event->timestamp_ns, event->qty);
            trade.has_value())
        {
          trades.push_back(*trade);
        }
      }
    }

    return trades;
  }

  bool MarketReplay::run_stream(OrderBook &book, SimExchange &sim,
                                const std::function<bool(const Trade &)> &on_trade)
  {
    std::ifstream input(csv_path_);
    if (!input.is_open())
    {
      throw std::runtime_error("Unable to open replay CSV: " + csv_path_.string());
    }

    std::string line;
    if (!std::getline(input, line))
    {
      return true;
    }

    while (std::getline(input, line))
    {
      const auto trimmed = trim(line);
      if (trimmed.empty())
      {
        continue;
      }

      const auto event = parse_event_line(trimmed);
      if (!event)
      {
        throw std::runtime_error("Malformed replay event: " + trimmed);
      }

      const auto action = upper(event->action);
      const double effective_qty = (action == "DELETE" || event->qty == 0.0) ? 0.0 : event->qty;
      book.apply_update(event->side, event->price, effective_qty);

      if (book.best_bid().has_value() && book.best_ask().has_value() &&
          *book.best_bid() >= *book.best_ask())
      {
        const auto aggressor_side = (event->side == Side::Bid) ? Side::Ask : Side::Bid;
        if (const auto trade = sim.match_crossing(book, aggressor_side, event->timestamp_ns, event->qty);
            trade.has_value())
        {
          if (!on_trade(*trade))
          {
            return false; // consumer asked to stop
          }
        }
      }
    }

    return true;
  }

} // namespace liquidbook
