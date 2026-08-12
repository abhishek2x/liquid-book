cmake_minimum_required(VERSION 3.22)

if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
  message(FATAL_ERROR "Benchmarks require Release build. Configure with -DCMAKE_BUILD_TYPE=Release")
endif()

add_executable(book_benchmark
    book_benchmark.cpp
)
target_link_libraries(book_benchmark PRIVATE
    benchmark::benchmark
    book_lib
)
target_include_directories(book_benchmark PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_executable(queue_benchmark
    queue_benchmark.cpp
)
target_link_libraries(queue_benchmark PRIVATE
    benchmark::benchmark
)
target_include_directories(queue_benchmark PRIVATE ${CMAKE_SOURCE_DIR}/include)
