#include "simulation_recorder.hpp"
#include <fstream>
#include <sstream>
#include <iostream>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(dir) _mkdir(dir)
#else
#include <sys/stat.h>
#define MKDIR(dir) mkdir(dir, 0755)
#endif

void SimulationRecorder::reset() {
    sequence_counter_ = 0;
    trades_.clear();
    outcomes_.clear();
    depth_snapshots_.clear();
    summary_ = SimulationSummary{};
}

void SimulationRecorder::process_event(const Event& event, MatchingEngine& engine) {
    ++sequence_counter_;
    summary_.total_events++;

    OutcomeRecord outcome;
    outcome.sequence_num = sequence_counter_;
    outcome.event_type = event.type;
    outcome.order_id = event.order.order_id;

    if (event.type == EventType::NewOrder) {
        summary_.new_orders++;
        outcome.side = event.order.side;
        outcome.order_type = event.order.type;
        outcome.price = event.order.price;
        outcome.initial_quantity = event.order.initial_quantity;

        OrderResult result = engine.process_order(event.order);
        outcome.status = result.status;
        outcome.executed_quantity = result.executed_quantity;
        outcome.remaining_quantity = result.remaining_quantity;
        outcome.cancelled_quantity = result.cancelled_quantity;
        outcome.reject_reason = result.reject_reason;

        if (result.is_accepted()) {
            summary_.accepted_events++;
        } else {
            summary_.rejected_events++;
        }

        for (const auto& trade : result.trades) {
            trades_.push_back(trade);
            summary_.total_trades++;
            summary_.total_executed_volume += trade.executed_quantity;
        }
    } else if (event.type == EventType::CancelOrder) {
        summary_.cancel_orders++;
        auto historical = engine.get_order_history(event.order.order_id);
        if (historical.has_value()) {
            outcome.side = historical->side;
            outcome.order_type = historical->type;
            outcome.price = historical->price;
        }

        OrderResult result = engine.cancel_order(event.order.order_id);
        outcome.status = result.status;
        outcome.executed_quantity = result.executed_quantity;
        outcome.remaining_quantity = result.remaining_quantity;
        outcome.cancelled_quantity = result.cancelled_quantity;
        outcome.reject_reason = result.reject_reason;

        if (result.is_accepted()) {
            summary_.accepted_events++;
        } else {
            summary_.rejected_events++;
        }
    }

    outcomes_.push_back(outcome);

    // Capture Order Book Depth Snapshot
    DepthSnapshotRecord snap;
    snap.sequence_num = sequence_counter_;
    snap.best_bid_price = engine.book().best_bid();
    if (snap.best_bid_price.has_value()) {
        auto b_levels = engine.get_bid_depth(1);
        if (!b_levels.empty()) {
            snap.best_bid_qty = b_levels[0].total_quantity;
        }
    }

    snap.best_ask_price = engine.book().best_ask();
    if (snap.best_ask_price.has_value()) {
        auto a_levels = engine.get_ask_depth(1);
        if (!a_levels.empty()) {
            snap.best_ask_qty = a_levels[0].total_quantity;
        }
    }

    if (snap.best_bid_price.has_value() && snap.best_ask_price.has_value()) {
        snap.spread = *snap.best_ask_price - *snap.best_bid_price;
    } else {
        snap.spread = NullOpt;
    }

    snap.total_bid_volume = engine.book().total_bid_volume();
    snap.total_ask_volume = engine.book().total_ask_volume();
    snap.active_orders_count = engine.book().size();

    depth_snapshots_.push_back(snap);
}

void SimulationRecorder::run_simulation(const std::vector<Event>& events, MatchingEngine& engine) {
    for (const auto& event : events) {
        process_event(event, engine);
    }

    summary_.final_active_orders = engine.book().size();
    summary_.final_total_bid_volume = engine.book().total_bid_volume();
    summary_.final_total_ask_volume = engine.book().total_ask_volume();
    summary_.final_best_bid = engine.book().best_bid();
    summary_.final_best_ask = engine.book().best_ask();
    summary_.invariants_passed = engine.check_invariants();
}

std::string SimulationRecorder::to_trades_csv() const {
    std::ostringstream ss;
    ss << "trade_id,timestamp,buyer_order_id,seller_order_id,price,quantity\n";
    for (const auto& t : trades_) {
        ss << t.trade_id << ","
           << t.timestamp << ","
           << t.buy_order_id << ","
           << t.sell_order_id << ","
           << t.execution_price << ","
           << t.executed_quantity << "\n";
    }
    return ss.str();
}

std::string SimulationRecorder::to_outcomes_csv() const {
    std::ostringstream ss;
    ss << "sequence_num,event_type,order_id,side,order_type,price,initial_quantity,status,executed_quantity,remaining_quantity,cancelled_quantity,reject_reason\n";
    for (const auto& o : outcomes_) {
        ss << o.sequence_num << ","
           << (o.event_type == EventType::NewOrder ? "NEW" : "CANCEL") << ","
           << o.order_id << ","
           << (o.side.has_value() ? (*o.side == Side::Buy ? "BUY" : "SELL") : "") << ","
           << (o.order_type == OrderType::Limit ? "LIMIT" : "MARKET") << ","
           << o.price << ","
           << o.initial_quantity << ","
           << o.status << ","
           << o.executed_quantity << ","
           << o.remaining_quantity << ","
           << o.cancelled_quantity << ","
           << o.reject_reason << "\n";
    }
    return ss.str();
}

std::string SimulationRecorder::to_depth_snapshots_csv() const {
    std::ostringstream ss;
    ss << "sequence_num,best_bid_price,best_bid_qty,best_ask_price,best_ask_qty,spread,total_bid_volume,total_ask_volume,active_orders\n";
    for (const auto& s : depth_snapshots_) {
        ss << s.sequence_num << ",";
        if (s.best_bid_price.has_value()) {
            ss << *s.best_bid_price;
        }
        ss << "," << s.best_bid_qty << ",";
        if (s.best_ask_price.has_value()) {
            ss << *s.best_ask_price;
        }
        ss << "," << s.best_ask_qty << ",";
        if (s.spread.has_value()) {
            ss << *s.spread;
        }
        ss << "," << s.total_bid_volume << ","
           << s.total_ask_volume << ","
           << s.active_orders_count << "\n";
    }
    return ss.str();
}

std::string SimulationRecorder::to_summary_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"total_events\": " << summary_.total_events << ",\n";
    ss << "  \"new_orders\": " << summary_.new_orders << ",\n";
    ss << "  \"cancel_orders\": " << summary_.cancel_orders << ",\n";
    ss << "  \"accepted_events\": " << summary_.accepted_events << ",\n";
    ss << "  \"rejected_events\": " << summary_.rejected_events << ",\n";
    ss << "  \"total_trades\": " << summary_.total_trades << ",\n";
    ss << "  \"total_executed_volume\": " << summary_.total_executed_volume << ",\n";
    ss << "  \"final_active_orders\": " << summary_.final_active_orders << ",\n";
    ss << "  \"final_total_bid_volume\": " << summary_.final_total_bid_volume << ",\n";
    ss << "  \"final_total_ask_volume\": " << summary_.final_total_ask_volume << ",\n";
    ss << "  \"final_best_bid\": ";
    if (summary_.final_best_bid.has_value()) {
        ss << *summary_.final_best_bid;
    } else {
        ss << "null";
    }
    ss << ",\n";
    ss << "  \"final_best_ask\": ";
    if (summary_.final_best_ask.has_value()) {
        ss << *summary_.final_best_ask;
    } else {
        ss << "null";
    }
    ss << ",\n";
    ss << "  \"invariants_passed\": " << (summary_.invariants_passed ? "true" : "false") << "\n";
    ss << "}\n";
    return ss.str();
}

bool SimulationRecorder::export_trades_csv(const std::string& filepath) const {
    std::ofstream out(filepath);
    if (!out.is_open()) return false;
    out << to_trades_csv();
    return true;
}

bool SimulationRecorder::export_outcomes_csv(const std::string& filepath) const {
    std::ofstream out(filepath);
    if (!out.is_open()) return false;
    out << to_outcomes_csv();
    return true;
}

bool SimulationRecorder::export_depth_snapshots_csv(const std::string& filepath) const {
    std::ofstream out(filepath);
    if (!out.is_open()) return false;
    out << to_depth_snapshots_csv();
    return true;
}

bool SimulationRecorder::export_summary_json(const std::string& filepath) const {
    std::ofstream out(filepath);
    if (!out.is_open()) return false;
    out << to_summary_json();
    return true;
}

bool SimulationRecorder::export_all(const std::string& output_directory) const {
    MKDIR(output_directory.c_str());
    std::string prefix = output_directory;
    if (!prefix.empty() && prefix.back() != '/' && prefix.back() != '\\') {
        prefix += "/";
    }

    bool success = true;
    if (!export_trades_csv(prefix + "trades.csv")) success = false;
    if (!export_outcomes_csv(prefix + "outcomes.csv")) success = false;
    if (!export_depth_snapshots_csv(prefix + "depth_snapshots.csv")) success = false;
    if (!export_summary_json(prefix + "run_summary.json")) success = false;
    return success;
}
