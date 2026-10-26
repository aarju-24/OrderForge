#ifndef BENCHMARK_HPP
#define BENCHMARK_HPP

#include <chrono>
#include <vector>
#include <iostream>
#include "matching_engine.hpp"

class Benchmark {
public:
    static void run(MatchingEngine& engine, size_t order_count) {
        std::cout << "\n===========================================\n";
        std::cout << "      OrderForge - Performance Benchmark   \n";
        std::cout << "===========================================\n";
        std::cout << "Generating " << order_count << " synthetic orders...\n";

        // Generate synthetic orders (alternating buy/sell)
        std::vector<Order> test_orders;
        test_orders.reserve(order_count);

        for (size_t i = 1; i <= order_count; ++i) {
            Side side = (i % 2 == 0) ? Side::Buy : Side::Sell;
            Price price = 15000 + (i % 10) * 10; // Prices ranging 15000-15090
            Quantity qty = 100;
            test_orders.push_back(Order{i, side, OrderType::Limit, price, qty, qty, OrderStatus::New});
        }

        std::cout << "Running benchmark sweep...\n";

        auto start = std::chrono::high_resolution_clock::now();

        size_t total_trades = 0;
        for (const auto& order : test_orders) {
            auto result = engine.process_order(order);
            total_trades += result.trades.size();
        }


        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;

        double ops_per_sec = (order_count / elapsed.count()) * 1000.0;
        double avg_latency_us = (elapsed.count() * 1000.0) / order_count;

        std::cout << "--- BENCHMARK RESULTS ---\n";
        std::cout << "Total Orders Processed : " << order_count << "\n";
        std::cout << "Total Trades Generated : " << total_trades << "\n";
        std::cout << "Total Time Elapsed     : " << elapsed.count() << " ms\n";
        std::cout << "Average Latency        : " << avg_latency_us << " us / order\n";
        std::cout << "Throughput             : " << static_cast<uint64_t>(ops_per_sec) << " orders/sec\n";
        std::cout << "===========================================\n";
    }
};

#endif // BENCHMARK_HPP
