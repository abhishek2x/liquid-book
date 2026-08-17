#include "book/order_book.hpp"
#include "sim/market_replay.hpp"
#include "risk/limits.hpp"
#include "queue/fast_queue.hpp"

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>

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
    std::atomic<bool> done(false);

    // FastQueue setup: capacity 4096, 1 consumer
    FastQueue<Trade, 4096, 1> queue(1);

    double position = 0.0;
    double pnl = 0.0; // placeholder: cumulative realized PnL

    // Consumer thread to process trade events and check risk limits
    std::thread consumer_thread([&]()
    {
        Trade t;
        while (!kill_switch.load(std::memory_order_relaxed))
        {
            if (queue.try_pop(0, t))
            {
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
                    break;
                }

                std::cout << "TRADE ts=" << t.timestamp_ns << " side=" << (t.aggressor_side == Side::Bid ? "B" : "A")
                          << " price=" << t.price << " qty=" << t.qty << "\n";
            }
            else
            {
                if (done.load(std::memory_order_relaxed))
                {
                    // Drain any remaining events pushed right before done was set
                    while (queue.try_pop(0, t))
                    {
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
                            break;
                        }

                        std::cout << "TRADE ts=" << t.timestamp_ns << " side=" << (t.aggressor_side == Side::Bid ? "B" : "A")
                                  << " price=" << t.price << " qty=" << t.qty << "\n";
                    }
                    break;
                }
                std::this_thread::yield();
            }
        }
    });

    auto on_trade = [&](const Trade &t) -> bool
    {
        if (kill_switch.load(std::memory_order_relaxed))
        {
            return false;
        }

        while (!queue.try_push(t))
        {
            if (kill_switch.load(std::memory_order_relaxed))
            {
                return false;
            }
            std::this_thread::yield();
        }

        return true;
    };

    try
    {
        const bool completed = replay.run_stream(book, sim, on_trade);
        done.store(true, std::memory_order_relaxed);
        consumer_thread.join();

        if (!completed || kill_switch.load(std::memory_order_relaxed))
        {
            std::cout << "Replay halted by consumer (risk)." << std::endl;
        }
    }
    catch (const std::exception &ex)
    {
        done.store(true);
        if (consumer_thread.joinable())
        {
            consumer_thread.join();
        }
        std::cerr << "Replay error: " << ex.what() << '\n';
        return EXIT_FAILURE;
    }

    std::cout << "Engine finished. final_position=" << position << " final_pnl=" << pnl << "\n";
    return EXIT_SUCCESS;
}
