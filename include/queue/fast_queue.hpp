#pragma once

#include <atomic>
#include <array>
#include <cstdint>
#include <cstddef>
#include <stdexcept>

namespace liquidbook
{

    struct alignas(64) AlignedCursor
    {
        std::atomic<uint64_t> value{0};
    };

    template <typename T, std::size_t Capacity, std::size_t MaxConsumers = 8>
    class FastQueue
    {
        static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");

    public:
        explicit FastQueue(std::size_t active_consumers = MaxConsumers)
            : active_consumers_(active_consumers)
        {
            if (active_consumers_ == 0 ||
                active_consumers_ > MaxConsumers)
            {
                throw std::invalid_argument(
                    "active_consumers must be between 1 and MaxConsumers");
            }
        }

        FastQueue(const FastQueue &) = delete;
        FastQueue &operator=(const FastQueue &) = delete;
        FastQueue(FastQueue &&) = delete;
        FastQueue &operator=(FastQueue &&) = delete;

        bool try_push(const T &item)
        {
            uint64_t write_pos = write_pos_.load(std::memory_order_relaxed);

            for (std::size_t i = 0; i < active_consumers_; ++i)
            {
                uint64_t read_pos = read_pos_[i].value.load(std::memory_order_acquire);
                if (write_pos - read_pos >= Capacity)
                {
                    // Do not overwrite data that this consumer has not read yet.
                    return false;
                }
            }

            storage_[write_pos & (Capacity - 1)] = item;
            write_pos_.store(write_pos + 1, std::memory_order_release);
            return true;
        }

        bool try_pop(std::size_t consumer_id, T &out)
        {
            if (consumer_id >= active_consumers_)
            {
                return false;
            }

            uint64_t read_pos = read_pos_[consumer_id].value.load(std::memory_order_relaxed);
            uint64_t write_pos = write_pos_.load(std::memory_order_acquire);

            if (read_pos == write_pos) // queue is empty for this consumer
            {
                return false;
            }

            out = storage_[read_pos & (Capacity - 1)];
            read_pos_[consumer_id].value.store(read_pos + 1, std::memory_order_release);
            return true;
        }

        void reset()
        {
            write_pos_.store(0, std::memory_order_relaxed);
            for (std::size_t i = 0; i < active_consumers_; ++i)
            {
                read_pos_[i].value.store(0, std::memory_order_relaxed);
            }
        }

    private:
        std::size_t active_consumers_;
        std::array<T, Capacity> storage_{};

        alignas(64) std::atomic<uint64_t> write_pos_{0};
        std::array<AlignedCursor, MaxConsumers> read_pos_{};
    };

} // namespace liquidbook
