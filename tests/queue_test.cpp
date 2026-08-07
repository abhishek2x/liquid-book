#include "queue/fast_queue.hpp"
#include <gtest/gtest.h>
#include <string>

using namespace liquidbook;

TEST(FastQueueCorrectness, SingleThreadedPushPop) {
    FastQueue<int, 4, 1> queue;
    int val = 0;

    // Initially empty
    EXPECT_FALSE(queue.try_pop(0, val));

    // Push 3 elements
    EXPECT_TRUE(queue.try_push(10));
    EXPECT_TRUE(queue.try_push(20));
    EXPECT_TRUE(queue.try_push(30));

    // Pop and verify order
    EXPECT_TRUE(queue.try_pop(0, val));
    EXPECT_EQ(val, 10);
    EXPECT_TRUE(queue.try_pop(0, val));
    EXPECT_EQ(val, 20);
    EXPECT_TRUE(queue.try_pop(0, val));
    EXPECT_EQ(val, 30);

    // Empty again
    EXPECT_FALSE(queue.try_pop(0, val));
}

TEST(FastQueueCorrectness, PushToFullCapacity) {
    FastQueue<int, 4, 1> queue;
    int val = 0;

    EXPECT_TRUE(queue.try_push(1));
    EXPECT_TRUE(queue.try_push(2));
    EXPECT_TRUE(queue.try_push(3));
    EXPECT_TRUE(queue.try_push(4));

    // Queue is now full (Capacity = 4)
    EXPECT_FALSE(queue.try_push(5));

    // Pop one element, freeing a slot
    EXPECT_TRUE(queue.try_pop(0, val));
    EXPECT_EQ(val, 1);

    // Push should succeed now
    EXPECT_TRUE(queue.try_push(5));
    EXPECT_FALSE(queue.try_push(6)); // Full again
}

TEST(FastQueueCorrectness, PopFromEmpty) {
    FastQueue<std::string, 8, 2> queue;
    std::string val;

    // Both consumers see empty queue
    EXPECT_FALSE(queue.try_pop(0, val));
    EXPECT_FALSE(queue.try_pop(1, val));
    EXPECT_FALSE(queue.try_pop(2, val)); // Out of bounds consumer ID returns false
}

TEST(FastQueueCorrectness, IndependentConsumerCursors) {
    FastQueue<int, 4, 2> queue;
    int val0 = 0;
    int val1 = 0;

    EXPECT_TRUE(queue.try_push(100));
    EXPECT_TRUE(queue.try_push(200));

    // Consumer 0 pops first item
    EXPECT_TRUE(queue.try_pop(0, val0));
    EXPECT_EQ(val0, 100);

    // Consumer 1 still sees first item (independent cursor)
    EXPECT_TRUE(queue.try_pop(1, val1));
    EXPECT_EQ(val1, 100);

    // Consumer 0 pops second item
    EXPECT_TRUE(queue.try_pop(0, val0));
    EXPECT_EQ(val0, 200);
    EXPECT_FALSE(queue.try_pop(0, val0)); // Empty for consumer 0

    // Consumer 1 pops second item
    EXPECT_TRUE(queue.try_pop(1, val1));
    EXPECT_EQ(val1, 200);
    EXPECT_FALSE(queue.try_pop(1, val1)); // Empty for consumer 1
}

TEST(FastQueueCorrectness, WrapAroundBoundary) {
    FastQueue<int, 4, 1> queue;
    int val = 0;

    // Fill queue
    EXPECT_TRUE(queue.try_push(1));
    EXPECT_TRUE(queue.try_push(2));
    EXPECT_TRUE(queue.try_push(3));
    EXPECT_TRUE(queue.try_push(4));
    EXPECT_FALSE(queue.try_push(5)); // Full

    // Pop two items
    EXPECT_TRUE(queue.try_pop(0, val));
    EXPECT_EQ(val, 1);
    EXPECT_TRUE(queue.try_pop(0, val));
    EXPECT_EQ(val, 2);

    // Push two more items (which wraps index from 3 to 4, 5, etc.)
    EXPECT_TRUE(queue.try_push(5));
    EXPECT_TRUE(queue.try_push(6));
    EXPECT_FALSE(queue.try_push(7)); // Full again

    // Pop the rest
    EXPECT_TRUE(queue.try_pop(0, val));
    EXPECT_EQ(val, 3);
    EXPECT_TRUE(queue.try_pop(0, val));
    EXPECT_EQ(val, 4);
    EXPECT_TRUE(queue.try_pop(0, val));
    EXPECT_EQ(val, 5);
    EXPECT_TRUE(queue.try_pop(0, val));
    EXPECT_EQ(val, 6);
    EXPECT_FALSE(queue.try_pop(0, val)); // Empty
}
