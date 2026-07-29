// book_test.cpp — Unit + property tests for the reverse-vector OrderBook.
//
// Test structure:
//   Section 1: Empty-book invariants
//   Section 2: Single-side basic operations (insert / update / delete)
//   Section 3: Both sides together, crossing detection
//   Section 4: Edge cases (drain-and-reinsert, large depth, etc.)
//   Section 5: Differential property test vs. ReferenceBook (N = 10,000 sequences)

#include "book/order_book.hpp"
#include "reference_book.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <random>
#include <tuple>
#include <vector>

using namespace liquidbook;

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static constexpr double EPS = 1e-9;

static bool dbl_eq(double a, double b) { return std::abs(a - b) < EPS; }

/// Verify that bid_levels_ is strictly ascending.
static void assert_bids_ascending(const OrderBook& book) {
    const auto& lvls = book.bid_levels();
    for (std::size_t i = 1; i < lvls.size(); ++i) {
        EXPECT_LT(lvls[i - 1].price, lvls[i].price)
            << "Bid levels not ascending at index " << i;
    }
}

/// Verify that ask_levels_ is strictly ascending.
static void assert_asks_ascending(const OrderBook& book) {
    const auto& lvls = book.ask_levels();
    for (std::size_t i = 1; i < lvls.size(); ++i) {
        EXPECT_LT(lvls[i - 1].price, lvls[i].price)
            << "Ask levels not ascending at index " << i;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Section 1: Empty-book invariants
// ─────────────────────────────────────────────────────────────────────────────

TEST(OrderBookEmpty, BestBidIsNulloptOnEmptyBook) {
    OrderBook book;
    EXPECT_FALSE(book.best_bid().has_value());
}

TEST(OrderBookEmpty, BestAskIsNulloptOnEmptyBook) {
    OrderBook book;
    EXPECT_FALSE(book.best_ask().has_value());
}

TEST(OrderBookEmpty, BestBidLevelIsNulloptOnEmptyBook) {
    OrderBook book;
    EXPECT_FALSE(book.best_bid_level().has_value());
}

TEST(OrderBookEmpty, BestAskLevelIsNulloptOnEmptyBook) {
    OrderBook book;
    EXPECT_FALSE(book.best_ask_level().has_value());
}

TEST(OrderBookEmpty, DepthIsZeroOnEmptyBook) {
    OrderBook book;
    EXPECT_EQ(book.bid_depth(), 0u);
    EXPECT_EQ(book.ask_depth(), 0u);
}

TEST(OrderBookEmpty, EmptyReturnsTrueOnEmptyBook) {
    OrderBook book;
    EXPECT_TRUE(book.empty());
}

TEST(OrderBookEmpty, DeleteOnEmptyBookIsNoOp) {
    // Deleting a non-existent price on an empty book must not crash or corrupt state.
    OrderBook book;
    EXPECT_NO_THROW(book.apply_update(Side::Bid, 100.0, 0.0));
    EXPECT_NO_THROW(book.apply_update(Side::Ask, 101.0, 0.0));
    EXPECT_TRUE(book.empty());
}

// ─────────────────────────────────────────────────────────────────────────────
// Section 2a: Bid side operations
// ─────────────────────────────────────────────────────────────────────────────

TEST(OrderBookBid, SingleInsert) {
    OrderBook book;
    book.apply_update(Side::Bid, 100.0, 50.0);

    ASSERT_TRUE(book.best_bid().has_value());
    EXPECT_TRUE(dbl_eq(*book.best_bid(), 100.0));
    EXPECT_EQ(book.bid_depth(), 1u);
    EXPECT_TRUE(book.ask_depth() == 0u);
}

TEST(OrderBookBid, UpdateExistingLevel) {
    OrderBook book;
    book.apply_update(Side::Bid, 100.0, 50.0);
    book.apply_update(Side::Bid, 100.0, 80.0);  // update qty

    EXPECT_EQ(book.bid_depth(), 1u);
    EXPECT_TRUE(dbl_eq(book.best_bid_level()->aggregate_qty, 80.0));
}

TEST(OrderBookBid, DeleteExistingLevel) {
    OrderBook book;
    book.apply_update(Side::Bid, 100.0, 50.0);
    book.apply_update(Side::Bid, 100.0, 0.0);   // delete

    EXPECT_EQ(book.bid_depth(), 0u);
    EXPECT_FALSE(book.best_bid().has_value());
}

TEST(OrderBookBid, DeleteNonExistentPriceIsNoOp) {
    OrderBook book;
    book.apply_update(Side::Bid, 100.0, 50.0);
    book.apply_update(Side::Bid, 99.0, 0.0);    // 99.0 was never inserted

    EXPECT_EQ(book.bid_depth(), 1u);
    EXPECT_TRUE(dbl_eq(*book.best_bid(), 100.0));
}

TEST(OrderBookBid, MultipleLevelsAscendingOrder) {
    OrderBook book;
    book.apply_update(Side::Bid, 99.0, 10.0);
    book.apply_update(Side::Bid, 101.0, 20.0);
    book.apply_update(Side::Bid, 100.0, 30.0);

    ASSERT_EQ(book.bid_depth(), 3u);
    EXPECT_TRUE(dbl_eq(*book.best_bid(), 101.0));  // highest price is best bid
    assert_bids_ascending(book);
}

TEST(OrderBookBid, BestBidUpdatesAfterDelete) {
    OrderBook book;
    book.apply_update(Side::Bid, 101.0, 20.0);
    book.apply_update(Side::Bid, 100.0, 30.0);
    book.apply_update(Side::Bid, 99.0,  10.0);

    book.apply_update(Side::Bid, 101.0, 0.0);  // delete best bid

    ASSERT_TRUE(book.best_bid().has_value());
    EXPECT_TRUE(dbl_eq(*book.best_bid(), 100.0));
    EXPECT_EQ(book.bid_depth(), 2u);
    assert_bids_ascending(book);
}

TEST(OrderBookBid, DrainAllLevels) {
    OrderBook book;
    book.apply_update(Side::Bid, 100.0, 10.0);
    book.apply_update(Side::Bid, 101.0, 20.0);
    book.apply_update(Side::Bid, 100.0, 0.0);
    book.apply_update(Side::Bid, 101.0, 0.0);

    EXPECT_EQ(book.bid_depth(), 0u);
    EXPECT_FALSE(book.best_bid().has_value());
}

// ─────────────────────────────────────────────────────────────────────────────
// Section 2b: Ask side operations
// ─────────────────────────────────────────────────────────────────────────────

TEST(OrderBookAsk, SingleInsert) {
    OrderBook book;
    book.apply_update(Side::Ask, 101.0, 30.0);

    ASSERT_TRUE(book.best_ask().has_value());
    EXPECT_TRUE(dbl_eq(*book.best_ask(), 101.0));
    EXPECT_EQ(book.ask_depth(), 1u);
}

TEST(OrderBookAsk, UpdateExistingLevel) {
    OrderBook book;
    book.apply_update(Side::Ask, 101.0, 30.0);
    book.apply_update(Side::Ask, 101.0, 60.0);

    EXPECT_EQ(book.ask_depth(), 1u);
    EXPECT_TRUE(dbl_eq(book.best_ask_level()->aggregate_qty, 60.0));
}

TEST(OrderBookAsk, DeleteExistingLevel) {
    OrderBook book;
    book.apply_update(Side::Ask, 101.0, 30.0);
    book.apply_update(Side::Ask, 101.0, 0.0);

    EXPECT_EQ(book.ask_depth(), 0u);
    EXPECT_FALSE(book.best_ask().has_value());
}

TEST(OrderBookAsk, MultipleLevelsAscendingOrder) {
    OrderBook book;
    book.apply_update(Side::Ask, 103.0, 10.0);
    book.apply_update(Side::Ask, 101.0, 20.0);
    book.apply_update(Side::Ask, 102.0, 30.0);

    ASSERT_EQ(book.ask_depth(), 3u);
    EXPECT_TRUE(dbl_eq(*book.best_ask(), 101.0));  // lowest price is best ask
    assert_asks_ascending(book);
}

TEST(OrderBookAsk, BestAskUpdatesAfterDelete) {
    OrderBook book;
    book.apply_update(Side::Ask, 101.0, 20.0);
    book.apply_update(Side::Ask, 102.0, 30.0);
    book.apply_update(Side::Ask, 103.0, 10.0);

    book.apply_update(Side::Ask, 101.0, 0.0);  // delete best ask

    ASSERT_TRUE(book.best_ask().has_value());
    EXPECT_TRUE(dbl_eq(*book.best_ask(), 102.0));
    EXPECT_EQ(book.ask_depth(), 2u);
    assert_asks_ascending(book);
}

// ─────────────────────────────────────────────────────────────────────────────
// Section 3: Both sides together
// ─────────────────────────────────────────────────────────────────────────────

TEST(OrderBookBothSides, NormalSpread) {
    OrderBook book;
    book.apply_update(Side::Bid, 100.0, 50.0);
    book.apply_update(Side::Ask, 101.0, 30.0);

    ASSERT_TRUE(book.best_bid().has_value());
    ASSERT_TRUE(book.best_ask().has_value());
    EXPECT_TRUE(dbl_eq(*book.best_bid(), 100.0));
    EXPECT_TRUE(dbl_eq(*book.best_ask(), 101.0));
    // Spread should be positive (no crossing).
    EXPECT_LT(*book.best_bid(), *book.best_ask());
}

TEST(OrderBookBothSides, CrossingOrdersDoNotCorruptState) {
    // The book receives a crossing update (bid >= ask).  It must record the state faithfully
    // without crashing or silently corrupting the vectors.  Matching is the sim exchange's job.
    OrderBook book;
    book.apply_update(Side::Ask, 100.0, 30.0);
    book.apply_update(Side::Bid, 101.0, 50.0);  // bid > ask — crossing

    // Book should still be structurally valid.
    EXPECT_EQ(book.bid_depth(), 1u);
    EXPECT_EQ(book.ask_depth(), 1u);
    EXPECT_TRUE(dbl_eq(*book.best_bid(), 101.0));
    EXPECT_TRUE(dbl_eq(*book.best_ask(), 100.0));
    assert_bids_ascending(book);
    assert_asks_ascending(book);
}

TEST(OrderBookBothSides, IndependentSidesAfterMixedUpdates) {
    OrderBook book;
    book.apply_update(Side::Bid, 100.0, 50.0);
    book.apply_update(Side::Bid, 99.0,  20.0);
    book.apply_update(Side::Ask, 101.0, 30.0);
    book.apply_update(Side::Ask, 102.0, 15.0);
    book.apply_update(Side::Bid, 100.0, 0.0);  // delete best bid
    book.apply_update(Side::Ask, 101.0, 45.0); // update best ask qty

    EXPECT_TRUE(dbl_eq(*book.best_bid(), 99.0));
    EXPECT_TRUE(dbl_eq(*book.best_ask(), 101.0));
    EXPECT_TRUE(dbl_eq(book.best_ask_level()->aggregate_qty, 45.0));
    EXPECT_EQ(book.bid_depth(), 1u);
    EXPECT_EQ(book.ask_depth(), 2u);
}

// ─────────────────────────────────────────────────────────────────────────────
// Section 4: Edge cases
// ─────────────────────────────────────────────────────────────────────────────

TEST(OrderBookEdge, DrainAndReinsertSamePrice) {
    // Level drained to zero then immediately re-inserted at same price.
    OrderBook book;
    book.apply_update(Side::Bid, 100.0, 50.0);
    book.apply_update(Side::Bid, 100.0, 0.0);   // delete
    book.apply_update(Side::Bid, 100.0, 25.0);  // re-insert

    EXPECT_EQ(book.bid_depth(), 1u);
    EXPECT_TRUE(dbl_eq(*book.best_bid(), 100.0));
    EXPECT_TRUE(dbl_eq(book.best_bid_level()->aggregate_qty, 25.0));
}

TEST(OrderBookEdge, LargeNumberOfLevels) {
    OrderBook book;
    const int N = 500;

    // Insert N bid levels at prices 1..N and N ask levels at N+1..2N.
    for (int i = 1; i <= N; ++i) {
        book.apply_update(Side::Bid, static_cast<double>(i),       10.0);
        book.apply_update(Side::Ask, static_cast<double>(N + i),   10.0);
    }

    EXPECT_EQ(book.bid_depth(), static_cast<std::size_t>(N));
    EXPECT_EQ(book.ask_depth(), static_cast<std::size_t>(N));
    EXPECT_TRUE(dbl_eq(*book.best_bid(), static_cast<double>(N)));
    EXPECT_TRUE(dbl_eq(*book.best_ask(), static_cast<double>(N + 1)));
    assert_bids_ascending(book);
    assert_asks_ascending(book);
}

TEST(OrderBookEdge, InsertAtFarEnd) {
    // Insert a very low bid and a very high ask (far from top-of-book).
    OrderBook book;
    book.apply_update(Side::Bid, 100.0, 10.0);
    book.apply_update(Side::Bid, 101.0, 10.0);
    book.apply_update(Side::Bid, 50.0,  5.0);   // far from best bid

    book.apply_update(Side::Ask, 110.0, 10.0);
    book.apply_update(Side::Ask, 109.0, 10.0);
    book.apply_update(Side::Ask, 200.0, 5.0);   // far from best ask

    EXPECT_EQ(book.bid_depth(), 3u);
    EXPECT_EQ(book.ask_depth(), 3u);
    EXPECT_TRUE(dbl_eq(*book.best_bid(), 101.0));
    EXPECT_TRUE(dbl_eq(*book.best_ask(), 109.0));
    assert_bids_ascending(book);
    assert_asks_ascending(book);
}

TEST(OrderBookEdge, OnlyOneSidePopulated) {
    OrderBook book;
    book.apply_update(Side::Bid, 100.0, 10.0);

    EXPECT_TRUE(book.best_bid().has_value());
    EXPECT_FALSE(book.best_ask().has_value());
    EXPECT_FALSE(book.empty());
}

TEST(OrderBookEdge, SideFromStringCaseInsensitive) {
    EXPECT_EQ(side_from_string("bid"), Side::Bid);
    EXPECT_EQ(side_from_string("BID"), Side::Bid);
    EXPECT_EQ(side_from_string("Bid"), Side::Bid);
    EXPECT_EQ(side_from_string("ask"), Side::Ask);
    EXPECT_EQ(side_from_string("ASK"), Side::Ask);
    EXPECT_THROW(side_from_string("foo"), std::invalid_argument);
}

// ─────────────────────────────────────────────────────────────────────────────
// Section 5: Differential property test (10,000 random sequences)
//
// For every operation we apply it to both the ReferenceBook and OrderBook and
// assert that best bid, best ask, and full depth snapshots are identical.
// ─────────────────────────────────────────────────────────────────────────────

namespace {

enum class Op { Insert, Update, Delete };

struct BookUpdate {
    Side   side;
    double price;
    double qty;   // 0 = delete
};

/// Generate a random update from a small, reproducible price universe so deletions
/// frequently hit existing levels (making the test meaningful).
BookUpdate random_update(std::mt19937& rng,
                         const std::vector<double>& bid_prices,
                         const std::vector<double>& ask_prices) {
    std::uniform_int_distribution<int> side_dist(0, 1);
    Side side = (side_dist(rng) == 0) ? Side::Bid : Side::Ask;

    const auto& price_pool = (side == Side::Bid) ? bid_prices : ask_prices;
    std::uniform_int_distribution<std::size_t> price_idx(0, price_pool.size() - 1);
    double price = price_pool[price_idx(rng)];

    // 25% chance of deletion (qty = 0), 75% insert/update
    std::uniform_int_distribution<int> op_dist(0, 3);
    double qty = (op_dist(rng) == 0) ? 0.0
                                     : static_cast<double>(std::uniform_int_distribution<int>(1, 500)(rng));

    return {side, price, qty};
}

}  // namespace

TEST(OrderBookProperty, DifferentialTestVsReferenceBook) {
    constexpr int N_SEQUENCES  = 10'000;
    constexpr unsigned SEED    = 42;

    // Small, overlapping-ish price universes so crossings happen too.
    const std::vector<double> bid_prices = {95.0, 97.0, 99.0, 100.0, 101.0, 102.0};
    const std::vector<double> ask_prices = {99.0, 100.0, 101.0, 102.0, 103.0, 105.0};

    std::mt19937 rng(SEED);
    OrderBook    book;
    ReferenceBook ref;

    int failures = 0;

    for (int i = 0; i < N_SEQUENCES; ++i) {
        auto [side, price, qty] = random_update(rng, bid_prices, ask_prices);

        // Apply to both
        book.apply_update(side, price, qty);
        if (side == Side::Bid) ref.apply_bid_update(price, qty);
        else                   ref.apply_ask_update(price, qty);

        // ── Check best bid ───────────────────────────────────────────────────
        auto book_bb = book.best_bid();
        auto ref_bb  = ref.best_bid();
        bool bb_match = (book_bb.has_value() == ref_bb.has_value()) &&
                        (!book_bb.has_value() || dbl_eq(*book_bb, *ref_bb));
        if (!bb_match) {
            ADD_FAILURE() << "Iteration " << i << ": best_bid mismatch. "
                          << "book=" << (book_bb ? std::to_string(*book_bb) : "null")
                          << " ref="  << (ref_bb  ? std::to_string(*ref_bb)  : "null");
            ++failures;
        }

        // ── Check best ask ───────────────────────────────────────────────────
        auto book_ba = book.best_ask();
        auto ref_ba  = ref.best_ask();
        bool ba_match = (book_ba.has_value() == ref_ba.has_value()) &&
                        (!book_ba.has_value() || dbl_eq(*book_ba, *ref_ba));
        if (!ba_match) {
            ADD_FAILURE() << "Iteration " << i << ": best_ask mismatch. "
                          << "book=" << (book_ba ? std::to_string(*book_ba) : "null")
                          << " ref="  << (ref_ba  ? std::to_string(*ref_ba)  : "null");
            ++failures;
        }

        // ── Check full depth snapshot ─────────────────────────────────────────
        if (book.bid_depth() != ref.bid_depth() || book.ask_depth() != ref.ask_depth()) {
            ADD_FAILURE() << "Iteration " << i << ": depth mismatch. "
                          << "book_bids=" << book.bid_depth() << " ref_bids=" << ref.bid_depth()
                          << " book_asks=" << book.ask_depth() << " ref_asks=" << ref.ask_depth();
            ++failures;
        }

        // Snapshot comparison (price only — qty should also match)
        auto ref_bids = ref.bid_snapshot();
        const auto& book_bids = book.bid_levels();
        if (ref_bids.size() == book_bids.size()) {
            for (std::size_t j = 0; j < ref_bids.size(); ++j) {
                // book_bids is stored ascending (lowest first), so its back is the best bid.
                // ref_bids is returned descending (highest/best first).
                std::size_t book_idx = book_bids.size() - 1 - j;
                if (!dbl_eq(ref_bids[j].first, book_bids[book_idx].price) ||
                    !dbl_eq(ref_bids[j].second, book_bids[book_idx].aggregate_qty)) {
                    ADD_FAILURE() << "Iteration " << i << ": bid snapshot mismatch at level " << j;
                    ++failures;
                    break;
                }
            }
        }

        auto ref_asks = ref.ask_snapshot();
        const auto& book_asks = book.ask_levels();
        if (ref_asks.size() == book_asks.size()) {
            for (std::size_t j = 0; j < ref_asks.size(); ++j) {
                if (!dbl_eq(ref_asks[j].first, book_asks[j].price) ||
                    !dbl_eq(ref_asks[j].second, book_asks[j].aggregate_qty)) {
                    ADD_FAILURE() << "Iteration " << i << ": ask snapshot mismatch at level " << j;
                    ++failures;
                    break;
                }
            }
        }

        // Structural invariants
        assert_bids_ascending(book);
        assert_asks_ascending(book);

        // Stop early to avoid flooding the failure log.
        if (failures > 20) {
            FAIL() << "Too many failures (" << failures << "), stopping early.";
            break;
        }
    }

    EXPECT_EQ(failures, 0) << "Total failures in " << N_SEQUENCES << " random sequences.";
}
