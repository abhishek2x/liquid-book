#include "queue/fast_queue.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <numeric>
#include <chrono>
#include <string>

using namespace liquidbook;

constexpr uint64_t DEFAULT_NUM_ITEMS = 10'000'000;
constexpr std::size_t QUEUE_CAPACITY = 4096;
constexpr std::size_t MAX_CONSUMERS = 8;

struct ThreadData {
    uint64_t checksum{0};
    uint64_t count{0};
};

int main(int argc, char* argv[]) {
    std::size_t num_consumers = 4;
    uint64_t num_items = DEFAULT_NUM_ITEMS;

    if (argc > 1) {
        num_consumers = std::stoul(argv[1]);
    }
    if (argc > 2) {
        num_items = std::stoull(argv[2]);
    }

    if (num_consumers > MAX_CONSUMERS || num_consumers == 0) {
        std::cerr << "Invalid number of consumers (must be between 1 and " << MAX_CONSUMERS << ")\n";
        return 1;
    }

    std::cout << "Starting SPMC FastQueue stress test:\n"
              << "  - Consumers: " << num_consumers << "\n"
              << "  - Items: " << num_items << "\n"
              << "  - Capacity: " << QUEUE_CAPACITY << std::endl;

    FastQueue<uint64_t, QUEUE_CAPACITY, MAX_CONSUMERS> queue(num_consumers);

    std::vector<std::thread> consumer_threads;
    std::vector<ThreadData> consumer_data(num_consumers);

    auto start_time = std::chrono::high_resolution_clock::now();

    // Start consumers
    for (std::size_t i = 0; i < num_consumers; ++i) {
        consumer_threads.emplace_back([&queue, &data = consumer_data[i], i, num_items]() {
            uint64_t popped_count = 0;
            uint64_t val = 0;
            while (popped_count < num_items) {
                if (queue.try_pop(i, val)) {
                    data.checksum += val;
                    data.count++;
                    popped_count++;
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }

    // Start producer
    std::thread producer_thread([&queue, num_items]() {
        for (uint64_t i = 1; i <= num_items; ++i) {
            while (!queue.try_push(i)) {
                std::this_thread::yield();
            }
        }
    });

    producer_thread.join();
    for (auto& t : consumer_threads) {
        t.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    // Verify results
    uint64_t expected_checksum = 0;
    for (uint64_t i = 1; i <= num_items; ++i) {
        expected_checksum += i;
    }

    bool success = true;
    for (std::size_t i = 0; i < num_consumers; ++i) {
        std::cout << "Consumer " << i << " -> count: " << consumer_data[i].count
                  << ", checksum: " << consumer_data[i].checksum;
        if (consumer_data[i].checksum == expected_checksum && consumer_data[i].count == num_items) {
            std::cout << " (SUCCESS)\n";
        } else {
            std::cout << " (FAILED! Expected checksum: " << expected_checksum
                      << ", expected count: " << num_items << ")\n";
            success = false;
        }
    }

    if (success) {
        std::cout << "All consumers validated successfully!\n";
        std::cout << "Total time: " << duration_ms << " ms\n";
        double ops_per_sec = (double)num_items / (duration_ms / 1000.0);
        std::cout << "Throughput: " << ops_per_sec / 1'000'000.0 << " million operations/sec\n";
        return 0;
    } else {
        return 1;
    }
}
