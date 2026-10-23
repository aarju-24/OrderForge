cmake_minimum_required(VERSION 3.20)
project(OrderForge LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(order_forge 
    src/main.cpp
    src/order_book.cpp
    src/matching_engine.cpp
    src/event_reader.cpp
)

target_include_directories(order_forge PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)