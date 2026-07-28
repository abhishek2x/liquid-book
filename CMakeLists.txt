cmake_minimum_required(VERSION 3.22)
project(liquid_book CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── macOS SDK header fix ─────────────────────────────────────────────────────
# AppleClang 21 on this machine does not find the C++ stdlib headers without
# explicit -isystem paths pointing into the CommandLineTools SDK.
if(APPLE)
    execute_process(
        COMMAND xcrun --show-sdk-path
        OUTPUT_VARIABLE MACOS_SDK_PATH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(MACOS_SDK_PATH)
        message(STATUS "macOS SDK: ${MACOS_SDK_PATH}")
        add_compile_options(
            -isystem ${MACOS_SDK_PATH}/usr/include/c++/v1
            -isystem ${MACOS_SDK_PATH}/usr/include
        )
    endif()
endif()

# Compiler warnings — treated as errors to keep the tree clean
add_compile_options(-Wall -Wextra -Wpedantic)

# ── Dependencies ─────────────────────────────────────────────────────────────
include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
# Prevent gtest from overriding our compiler/linker settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# ── ThreadSanitizer option (Phase 2) ────────────────────────────────────────
option(ENABLE_TSAN "Build with ThreadSanitizer" OFF)
if(ENABLE_TSAN)
    message(STATUS "ThreadSanitizer enabled")
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
    add_link_options(-fsanitize=thread)
endif()

# ── AddressSanitizer option ──────────────────────────────────────────────────
option(ENABLE_ASAN "Build with AddressSanitizer" OFF)
if(ENABLE_ASAN)
    message(STATUS "AddressSanitizer enabled")
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

# ── Global include path ──────────────────────────────────────────────────────
include_directories(include)

# ── Order book library (shared between tests and future engine) ──────────────
add_library(book_lib
    src/book/order_book.cpp
)
target_include_directories(book_lib PUBLIC include)

# ── Sub-directories ──────────────────────────────────────────────────────────
enable_testing()
add_subdirectory(tests)
