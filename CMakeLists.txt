cmake_minimum_required(VERSION 3.20)
project(OrderForge LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# Compiler warnings configuration
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(MINGW)
        add_compile_options(-U__STRICT_ANSI__)
    endif()
endif()




if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()

# Core OrderForge library (domain models, order book, matching engine, reader, recorder)
add_library(order_forge_core STATIC
    src/order_book.cpp
    src/matching_engine.cpp
    src/event_reader.cpp
    src/simulation_recorder.cpp
)

target_include_directories(order_forge_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

# Main CLI executable
add_executable(order_forge
    src/main.cpp
)

target_link_libraries(order_forge PRIVATE order_forge_core)

# Performance Benchmark Executable
add_executable(order_forge_benchmark
    benchmarks/run_benchmarks.cpp
)

target_link_libraries(order_forge_benchmark PRIVATE order_forge_core)

# -------------------------------------------------------------------------
# Automated Testing with GoogleTest
# -------------------------------------------------------------------------
option(BUILD_TESTING "Build the test suite" ON)

if(BUILD_TESTING)
    enable_testing()
    include(FetchContent)

    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    )
    FetchContent_MakeAvailable(googletest)

    add_executable(order_forge_tests
        tests/test_order_book.cpp
        tests/test_matching_engine.cpp
        tests/test_event_reader.cpp
        tests/test_differential.cpp
        tests/test_replay.cpp
    )


    target_link_libraries(order_forge_tests PRIVATE
        order_forge_core
        gtest_main
    )

    include(GoogleTest)
    gtest_discover_tests(order_forge_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()