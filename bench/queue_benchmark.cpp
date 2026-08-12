#include <benchmark/benchmark.h>
#include "queue/fast_queue.hpp"

using namespace liquidbook;

static void BM_FastQueue_PublishConsume_OneProducerOneConsumer(benchmark::State &state)
{
    // Use a large capacity to avoid fill contention in this microbenchmark
    constexpr std::size_t Capacity = 1 << 16;
    FastQueue<int, Capacity, 1> q(1);
    int out = 0;

    for (auto _ : state)
    {
        q.try_push(42);
        q.try_pop(0, out);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_FastQueue_PublishConsume_OneProducerOneConsumer)->Unit(benchmark::kNanosecond);

static void BM_FastQueue_Publish(benchmark::State &state)
{
    constexpr std::size_t Capacity = 1 << 16;
    FastQueue<int, Capacity, 1> q(1);

    for (auto _ : state)
    {
        q.try_push(123);
    }
}
BENCHMARK(BM_FastQueue_Publish)->Unit(benchmark::kNanosecond);

BENCHMARK_MAIN();
