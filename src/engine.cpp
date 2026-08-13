#include "book/order_book.hpp"
#include "sim/market_replay.hpp"
#include "risk/limits.hpp"

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>w

using namespace liquidbook;

static void print_usage(const char *prog)
{
    std::cerr << "Usage: " << prog << " <replay_csv> [--max-pos N] [--max-drawdown N]\n";
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    std::string csv_path = argv[1];

    double max_pos = 1000.0;    // default limits
    double max_drawdown = 1e12; // effectively disabled by default

    for (int i = 2; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--max-pos") == 0 && i + 1 < argc)
        {
            max_pos = std::atof(argv[++i]);
        }
        else if (std::strcmp(argv[i], "--max-drawdown") == 0 && i + 1 < argc)
        {
            max_drawdown = std::atof(argv[++i]);
        }
        else
        {
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    OrderBook book;
    SimExchange sim;
    MarketReplay replay(csv_path);

    risk::RiskLimits limits{max_pos, max_drawdown};
    risk::RiskGuard guard(limits);

    std::atomic<bool> kill_switch(false);

    double position = 0.0;
    double pnl = 0.0; // placeholder: cumulative realized PnL

    auto on_trade = [&](const Trade &t) -> bool
    {
        // Update position: aggressor==Bid means buy (increase position)
        if (t.aggressor_side == Side::Bid)
        {
            position += t.qty;
        }
        else
        {
            position -= t.qty;
        }

        if (!guard.check(position, pnl))
        {
            std::cout << "Risk breach detected: position=" << position << " pnl=" << pnl << '\n';
            kill_switch.store(true, std::memory_order_relaxed);
            return false; // stop replay
        }

        std::cout << "TRADE ts=" << t.timestamp_ns << " side=" << (t.aggressor_side == Side::Bid ? "B" : "A")
                  << " price=" << t.price << " qty=" << t.qty << "\n";
        return true; // continue
    };

    try
    {
        const bool completed = replay.run_stream(book, sim, on_trade);
        if (!completed)
        {
            std::cout << "Replay halted by consumer (risk)." << std::endl;
        }
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Replay error: " << ex.what() << '\n';
        return EXIT_FAILURE;
    }

    std::cout << "Engine finished. final_position=" << position << " final_pnl=" << pnl << "\n";
    return EXIT_SUCCESS;
}
