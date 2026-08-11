#include "book/order_book.hpp"
#include "sim/market_replay.hpp"
#include "sim/sim_exchange.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

using namespace liquidbook;

TEST(SimExchange, MatchCrossingConsumesRestingQty)
{
    OrderBook book;
    book.apply_update(Side::Bid, 100.0, 50.0);
    book.apply_update(Side::Ask, 99.0, 30.0);

    SimExchange exchange;
    auto trade = exchange.match_crossing(book, Side::Ask, 42ULL);

    ASSERT_TRUE(trade.has_value());
    EXPECT_EQ(trade->aggressor_side, Side::Ask);
    EXPECT_DOUBLE_EQ(trade->price, 100.0);
    EXPECT_DOUBLE_EQ(trade->qty, 30.0);
    EXPECT_TRUE(book.best_bid().has_value());
    EXPECT_DOUBLE_EQ(*book.best_bid(), 100.0);
    EXPECT_DOUBLE_EQ(book.best_bid_level()->aggregate_qty, 20.0);
    EXPECT_FALSE(book.best_ask().has_value());
}

TEST(MarketReplay, ReplayProcessesDeterministicCrossings)
{
    const auto tmp_dir = std::filesystem::temp_directory_path();
    const auto csv_path = tmp_dir / "liquid_book_phase3_replay.csv";

    {
        std::ofstream csv(csv_path);
        csv << "timestamp_ns,side,price,qty,action\n";
        csv << "1,BID,100,50,INSERT\n";
        csv << "2,ASK,100,30,INSERT\n";
        csv << "3,ASK,101,10,INSERT\n";
    }

    MarketReplay replay(csv_path);
    OrderBook book;
    SimExchange exchange;
    const auto trades = replay.run(book, exchange);

    EXPECT_EQ(trades.size(), 1u);
    EXPECT_DOUBLE_EQ(trades[0].price, 100.0);
    EXPECT_DOUBLE_EQ(trades[0].qty, 30.0);
    ASSERT_TRUE(book.best_bid().has_value());
    EXPECT_DOUBLE_EQ(*book.best_bid(), 100.0);
    EXPECT_DOUBLE_EQ(book.best_bid_level()->aggregate_qty, 20.0);
    ASSERT_TRUE(book.best_ask().has_value());
    EXPECT_DOUBLE_EQ(*book.best_ask(), 101.0);
    EXPECT_DOUBLE_EQ(book.best_ask_level()->aggregate_qty, 10.0);

    std::filesystem::remove(csv_path);
}
