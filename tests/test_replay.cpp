#include <gtest/gtest.h>
#include <fstream>
#include <random>
#include "matching_engine.hpp"
#include "event_reader.hpp"
#include "simulation_recorder.hpp"

// Test 1: Deterministic replay on sample events.csv produces identical results
TEST(ReplayTest, SampleEventsDeterministicReplay) {
    auto events = EventReader::read_csv("data/events.csv");
    ASSERT_FALSE(events.empty());

    // First run from fresh state
    MatchingEngine engine1;
    SimulationRecorder recorder1;
    recorder1.run_simulation(events, engine1);

    // Second run from fresh state
    MatchingEngine engine2;
    SimulationRecorder recorder2;
    recorder2.run_simulation(events, engine2);

    // 1. Verify in-memory trades match
    ASSERT_EQ(recorder1.trades().size(), recorder2.trades().size());
    for (size_t i = 0; i < recorder1.trades().size(); ++i) {
        const auto& t1 = recorder1.trades()[i];
        const auto& t2 = recorder2.trades()[i];
        EXPECT_EQ(t1.trade_id, t2.trade_id);
        EXPECT_EQ(t1.buy_order_id, t2.buy_order_id);
        EXPECT_EQ(t1.sell_order_id, t2.sell_order_id);
        EXPECT_EQ(t1.execution_price, t2.execution_price);
        EXPECT_EQ(t1.executed_quantity, t2.executed_quantity);
        EXPECT_EQ(t1.timestamp, t2.timestamp);
    }

    // 2. Verify in-memory outcomes match
    ASSERT_EQ(recorder1.outcomes().size(), recorder2.outcomes().size());
    for (size_t i = 0; i < recorder1.outcomes().size(); ++i) {
        const auto& o1 = recorder1.outcomes()[i];
        const auto& o2 = recorder2.outcomes()[i];
        EXPECT_EQ(o1.sequence_num, o2.sequence_num);
        EXPECT_EQ(o1.order_id, o2.order_id);
        EXPECT_EQ(o1.status, o2.status);
        EXPECT_EQ(o1.executed_quantity, o2.executed_quantity);
        EXPECT_EQ(o1.remaining_quantity, o2.remaining_quantity);
        EXPECT_EQ(o1.cancelled_quantity, o2.cancelled_quantity);
        EXPECT_EQ(o1.reject_reason, o2.reject_reason);
    }

    // 3. Verify in-memory depth snapshots match
    ASSERT_EQ(recorder1.depth_snapshots().size(), recorder2.depth_snapshots().size());
    for (size_t i = 0; i < recorder1.depth_snapshots().size(); ++i) {
        const auto& s1 = recorder1.depth_snapshots()[i];
        const auto& s2 = recorder2.depth_snapshots()[i];
        EXPECT_EQ(s1.sequence_num, s2.sequence_num);
        EXPECT_EQ(s1.best_bid_price.has_value(), s2.best_bid_price.has_value());
        if (s1.best_bid_price.has_value()) {
            EXPECT_EQ(*s1.best_bid_price, *s2.best_bid_price);
        }
        EXPECT_EQ(s1.best_bid_qty, s2.best_bid_qty);
        EXPECT_EQ(s1.best_ask_price.has_value(), s2.best_ask_price.has_value());
        if (s1.best_ask_price.has_value()) {
            EXPECT_EQ(*s1.best_ask_price, *s2.best_ask_price);
        }
        EXPECT_EQ(s1.best_ask_qty, s2.best_ask_qty);
        EXPECT_EQ(s1.total_bid_volume, s2.total_bid_volume);
        EXPECT_EQ(s1.total_ask_volume, s2.total_ask_volume);
        EXPECT_EQ(s1.active_orders_count, s2.active_orders_count);
    }

    // 4. Verify formatted CSV and JSON outputs match byte-for-byte
    EXPECT_EQ(recorder1.to_trades_csv(), recorder2.to_trades_csv());
    EXPECT_EQ(recorder1.to_outcomes_csv(), recorder2.to_outcomes_csv());
    EXPECT_EQ(recorder1.to_depth_snapshots_csv(), recorder2.to_depth_snapshots_csv());
    EXPECT_EQ(recorder1.to_summary_json(), recorder2.to_summary_json());

    // 5. Invariants pass
    EXPECT_TRUE(recorder1.summary().invariants_passed);
    EXPECT_TRUE(recorder2.summary().invariants_passed);
}

// Test 2: Randomized event stream deterministic replay
TEST(ReplayTest, RandomizedStreamDeterministicReplay) {
    // Generate 500 deterministic events with fixed seed
    std::mt19937_64 rng(4242);
    std::uniform_int_distribution<int> type_dist(0, 10);
    std::uniform_int_distribution<int> side_dist(0, 1);
    std::uniform_int_distribution<int> price_dist(95, 105);
    std::uniform_int_distribution<Quantity> qty_dist(10, 100);

    std::vector<Event> stream;
    stream.reserve(500);

    OrderId next_id = 1;
    std::vector<OrderId> active_ids;

    for (int i = 0; i < 500; ++i) {
        int action = type_dist(rng);
        if (action < 8 || active_ids.empty()) {
            // New Order (80% chance)
            OrderId id = next_id++;
            Side side = (side_dist(rng) == 0) ? Side::Buy : Side::Sell;
            Price price = price_dist(rng) * 10;
            Quantity qty = qty_dist(rng);
            OrderType otype = (action == 7) ? OrderType::Market : OrderType::Limit;

            Order ord{id, side, otype, price, qty, qty, OrderStatus::New};
            stream.push_back(Event{static_cast<uint64_t>(i + 1), EventType::NewOrder, ord, static_cast<size_t>(i + 1)});
            active_ids.push_back(id);
        } else {
            // Cancel Order (20% chance)
            std::uniform_int_distribution<size_t> idx_dist(0, active_ids.size() - 1);
            size_t idx = idx_dist(rng);
            OrderId target_id = active_ids[idx];
            active_ids.erase(active_ids.begin() + idx);

            Order cancel_ord{};
            cancel_ord.order_id = target_id;
            stream.push_back(Event{static_cast<uint64_t>(i + 1), EventType::CancelOrder, cancel_ord, static_cast<size_t>(i + 1)});
        }
    }

    // Run replay 1
    MatchingEngine engine1;
    SimulationRecorder recorder1;
    recorder1.run_simulation(stream, engine1);

    // Run replay 2
    MatchingEngine engine2;
    SimulationRecorder recorder2;
    recorder2.run_simulation(stream, engine2);

    // Assert exact byte-for-byte reproducibility
    EXPECT_EQ(recorder1.to_trades_csv(), recorder2.to_trades_csv());
    EXPECT_EQ(recorder1.to_outcomes_csv(), recorder2.to_outcomes_csv());
    EXPECT_EQ(recorder1.to_depth_snapshots_csv(), recorder2.to_depth_snapshots_csv());
    EXPECT_EQ(recorder1.to_summary_json(), recorder2.to_summary_json());

    EXPECT_TRUE(recorder1.summary().invariants_passed);
    EXPECT_TRUE(recorder2.summary().invariants_passed);
    EXPECT_GT(recorder1.summary().total_trades, 0u);
}

// Test 3: Export files to disk and verify headers
TEST(ReplayTest, ExportFilesToDisk) {
    auto events = EventReader::read_csv("data/events.csv");
    MatchingEngine engine;
    SimulationRecorder recorder;
    recorder.run_simulation(events, engine);

    bool ok = recorder.export_all("output");
    EXPECT_TRUE(ok);

    // Check trades.csv header
    std::ifstream trades_file("output/trades.csv");
    ASSERT_TRUE(trades_file.is_open());
    std::string header;
    std::getline(trades_file, header);
    EXPECT_EQ(header, "trade_id,timestamp,buyer_order_id,seller_order_id,price,quantity");

    // Check outcomes.csv header
    std::ifstream outcomes_file("output/outcomes.csv");
    ASSERT_TRUE(outcomes_file.is_open());
    std::getline(outcomes_file, header);
    EXPECT_EQ(header, "sequence_num,event_type,order_id,side,order_type,price,initial_quantity,status,executed_quantity,remaining_quantity,cancelled_quantity,reject_reason");

    // Check depth_snapshots.csv header
    std::ifstream depth_file("output/depth_snapshots.csv");
    ASSERT_TRUE(depth_file.is_open());
    std::getline(depth_file, header);
    EXPECT_EQ(header, "sequence_num,best_bid_price,best_bid_qty,best_ask_price,best_ask_qty,spread,total_bid_volume,total_ask_volume,active_orders");

    // Check run_summary.json exists
    std::ifstream summary_file("output/run_summary.json");
    ASSERT_TRUE(summary_file.is_open());
}
