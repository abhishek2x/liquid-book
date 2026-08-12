#include <benchmark/benchmark.h>
#include "book/order_book.hpp"

using namespace liquidbook;

static void BM_BookUpdate_TopOfBook(benchmark::State &state)
{
    OrderBook base;
    const int N = 1024;
    // populate bids
    for (int i = 0; i < N; ++i)
    {
        base.apply_update(Side::Bid, 100.0 + i, 10.0);
    }

    const double top_price = *base.best_bid();

    for (auto _ : state)
    {
        OrderBook b = base; // copy to keep iterations comparable
        b.apply_update(Side::Bid, top_price, 11.0);
    }
}
BENCHMARK(BM_BookUpdate_TopOfBook)->Unit(benchmark::kNanosecond);

static void BM_BookUpdate_MidBook(benchmark::State &state)
{
    OrderBook base;
    const int N = 1024;
    for (int i = 0; i < N; ++i)
    {
        base.apply_update(Side::Bid, 100.0 + i, 10.0);
    }
    const double mid_price = 100.0 + N / 2;

    for (auto _ : state)
    {
        OrderBook b = base;
        b.apply_update(Side::Bid, mid_price, 5.0);
    }
}
BENCHMARK(BM_BookUpdate_MidBook)->Unit(benchmark::kNanosecond);

static void BM_BookUpdate_Delete(benchmark::State &state)
{
    OrderBook base;
    const int N = 1024;
    for (int i = 0; i < N; ++i)
    {
        base.apply_update(Side::Ask, 200.0 + i, 10.0);
    }
    const double del_price = 200.0 + N / 4;

    for (auto _ : state)
    {
        OrderBook b = base;
        b.apply_update(Side::Ask, del_price, 0.0);
    }
}
BENCHMARK(BM_BookUpdate_Delete)->Unit(benchmark::kNanosecond);

static void BM_BestBidAsk(benchmark::State &state)
{
    OrderBook b;
    for (int i = 0; i < 512; ++i)
    {
        b.apply_update(Side::Bid, 100.0 + i, 10.0);
        b.apply_update(Side::Ask, 200.0 + i, 10.0);
    }

    for (auto _ : state)
    {
        benchmark::DoNotOptimize(b.best_bid());
        benchmark::DoNotOptimize(b.best_ask());
    }
}
BENCHMARK(BM_BestBidAsk)->Unit(benchmark::kNanosecond);

BENCHMARK_MAIN();
