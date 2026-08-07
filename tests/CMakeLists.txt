cmake_minimum_required(VERSION 3.22)

# Phase 1 — Order book tests
add_executable(book_tests
    book_test.cpp
)
target_link_libraries(book_tests PRIVATE
    book_lib
    GTest::gtest_main
)
target_include_directories(book_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}   # for reference_book.hpp
)

include(GoogleTest)
gtest_discover_tests(book_tests)

# Phase 2 — Queue tests
add_executable(queue_tests
    queue_test.cpp
)
target_link_libraries(queue_tests PRIVATE
    GTest::gtest_main
)
gtest_discover_tests(queue_tests)

# Multi-threaded queue stress test (TSan target)
add_executable(queue_stress_test
    queue_stress_test.cpp
)
find_package(Threads REQUIRED)
target_link_libraries(queue_stress_test PRIVATE
    Threads::Threads
)

