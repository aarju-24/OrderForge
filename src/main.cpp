#include <iostream>
#include "matching_engine.hpp"
#include "event_reader.hpp"
#include "benchmark.hpp"

int main() {
    MatchingEngine engine;
    auto events = EventReader::read_csv("data/events.csv");

    std::cout << "===========================================\n";
    std::cout << "      OrderForge Engine - Event Replay     \n";
    std::cout << "===========================================\n";
    std::cout << "Loaded " << events.size() << " market events from data/events.csv\n\n";

    for (const auto& event : events) {
        if (event.type == EventType::NewOrder) {
            std::cout << "[EVENT] New Order ID: " << event.order.order_id 
                      << " (" << event.order.side << " " << event.order.initial_quantity 
                      << " @ " << event.order.price << " ticks)\n";
            
            auto trades = engine.process_order(event.order);
            for (const auto& trade : trades) {
                std::cout << "   -> [TRADE] Buyer: " << trade.buy_order_id 
                          << " | Seller: " << trade.sell_order_id 
                          << " | Qty: " << trade.executed_quantity 
                          << " @ " << trade.execution_price << " ticks\n";
            }
        } else if (event.type == EventType::CancelOrder) {
            std::cout << "[EVENT] Cancel Order ID: " << event.order.order_id << "\n";
            bool success = engine.cancel_order(event.order.order_id);
            std::cout << "   -> Cancellation Result: " << (success ? "SUCCESS" : "FAILED") << "\n";
        }
    }

    std::cout << "\n--- FINAL ORDER BOOK STATE ---\n";
    std::cout << "Active Orders : " << engine.book().size() << "\n";
    std::cout << "Best Bid      : " << engine.book().best_bid() << " ticks\n";
    std::cout << "Best Ask      : " << engine.book().best_ask() << " ticks\n";

    // Run performance benchmark with 100,000 synthetic orders
    MatchingEngine perf_engine;
    Benchmark::run(perf_engine, 100000);

    return 0;
}