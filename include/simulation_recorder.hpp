#ifndef SIMULATION_RECORDER_HPP
#define SIMULATION_RECORDER_HPP

#include <string>
#include <vector>
#include "types.hpp"
#include "trade.hpp"
#include "order.hpp"
#include "order_result.hpp"
#include "event_reader.hpp"
#include "matching_engine.hpp"

// Structured record for an individual event's lifecycle outcome
struct OutcomeRecord {
    uint64_t sequence_num{0};
    EventType event_type{EventType::NewOrder};
    OrderId order_id{0};
    Optional<Side> side{NullOpt};
    OrderType order_type{OrderType::Limit};
    Price price{0};
    Quantity initial_quantity{0};
    OrderStatus status{OrderStatus::New};
    Quantity executed_quantity{0};
    Quantity remaining_quantity{0};
    Quantity cancelled_quantity{0};
    std::string reject_reason;
};

// Structured record for order book state after each event is processed
struct DepthSnapshotRecord {
    uint64_t sequence_num{0};
    Optional<Price> best_bid_price{NullOpt};
    Quantity best_bid_qty{0};
    Optional<Price> best_ask_price{NullOpt};
    Quantity best_ask_qty{0};
    Optional<Price> spread{NullOpt};
    Quantity total_bid_volume{0};
    Quantity total_ask_volume{0};
    size_t active_orders_count{0};
};

// Summary metrics aggregating the full simulation run
struct SimulationSummary {
    uint64_t total_events{0};
    uint64_t new_orders{0};
    uint64_t cancel_orders{0};
    uint64_t accepted_events{0};
    uint64_t rejected_events{0};
    uint64_t total_trades{0};
    uint64_t total_executed_volume{0};
    size_t final_active_orders{0};
    Quantity final_total_bid_volume{0};
    Quantity final_total_ask_volume{0};
    Optional<Price> final_best_bid{NullOpt};
    Optional<Price> final_best_ask{NullOpt};
    bool invariants_passed{false};
};

// Class responsible for running event streams, capturing deterministic history,
// and exporting structured outputs (trades, outcomes, depth snapshots, run summary).
class SimulationRecorder {
public:
    SimulationRecorder() = default;

    // Reset all internal logs and counters
    void reset();

    // Process a single market event through the engine and record its outcomes and book snapshot
    void process_event(const Event& event, MatchingEngine& engine);

    // Execute an entire stream of events through the engine
    void run_simulation(const std::vector<Event>& events, MatchingEngine& engine);

    // Query in-memory records
    const std::vector<Trade>& trades() const { return trades_; }
    const std::vector<OutcomeRecord>& outcomes() const { return outcomes_; }
    const std::vector<DepthSnapshotRecord>& depth_snapshots() const { return depth_snapshots_; }
    const SimulationSummary& summary() const { return summary_; }

    // String formatting (CSV and JSON)
    std::string to_trades_csv() const;
    std::string to_outcomes_csv() const;
    std::string to_depth_snapshots_csv() const;
    std::string to_summary_json() const;

    // File exporters
    bool export_trades_csv(const std::string& filepath) const;
    bool export_outcomes_csv(const std::string& filepath) const;
    bool export_depth_snapshots_csv(const std::string& filepath) const;
    bool export_summary_json(const std::string& filepath) const;
    bool export_all(const std::string& output_directory) const;

private:
    uint64_t sequence_counter_{0};
    std::vector<Trade> trades_;
    std::vector<OutcomeRecord> outcomes_;
    std::vector<DepthSnapshotRecord> depth_snapshots_;
    SimulationSummary summary_;
};

#endif // SIMULATION_RECORDER_HPP
