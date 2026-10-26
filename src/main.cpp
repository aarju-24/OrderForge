#include <iostream>
#include <iomanip>
#include "matching_engine.hpp"
#include "event_reader.hpp"
#include "simulation_recorder.hpp"

int main(int argc, char* argv[]) {
    std::string csv_path = "data/events.csv";
    if (argc > 1) {
        csv_path = argv[1];
    }

    std::cout << "========================================================\n";
    std::cout << "          OrderForge - Limit Order Book Engine          \n";
    std::cout << "========================================================\n";
    std::cout << "Loading event log from: " << csv_path << "\n";

    ParseResult parse_res = EventReader::parse_csv(csv_path);
    if (!parse_res.errors.empty()) {
        std::cerr << "[Warning] Encountered " << parse_res.errors.size() << " parse errors:\n";
        for (const auto& err : parse_res.errors) {
            std::cerr << "  Line " << err.line_number << ": " << err.error_message 
                      << " (Raw: \"" << err.raw_line << "\")\n";
        }
    }

    if (parse_res.events.empty()) {
        std::cerr << "[Error] No valid events could be loaded from " << csv_path << "\n";
        return 1;
    }

    std::cout << "Successfully parsed " << parse_res.events.size() << " valid events.\n\n";

    MatchingEngine engine;
    SimulationRecorder recorder;

    // Process events sequentially and print trace
    for (const auto& event : parse_res.events) {
        if (event.type == EventType::NewOrder) {
            std::cout << "[EVENT " << event.sequence_number << "] NEW ORDER #" << event.order.order_id 
                      << " " << event.order.side << " " << event.order.initial_quantity 
                      << " @ " << event.order.price << " ticks (" << event.order.type << ")\n";
        } else {
            std::cout << "[EVENT " << event.sequence_number << "] CANCEL ORDER #" << event.order.order_id << "\n";
        }

        recorder.process_event(event, engine);

        const auto& outcome = recorder.outcomes().back();
        std::cout << "   -> Status: " << outcome.status 
                  << " | Executed: " << outcome.executed_quantity 
                  << " | Remaining: " << outcome.remaining_quantity 
                  << " | Cancelled: " << outcome.cancelled_quantity;
        if (!outcome.reject_reason.empty()) {
            std::cout << " (" << outcome.reject_reason << ")";
        }
        std::cout << "\n";

        // Print trades executed in this step
        for (const auto& t : recorder.trades()) {
            if (t.timestamp == recorder.trades().back().timestamp && !recorder.trades().empty()) {
                // If trade corresponds to the latest match
            }
        }
    }

    // Final state evaluation
    const auto& summary = recorder.summary();
    std::cout << "\n========================================================\n";
    std::cout << "                   SIMULATION SUMMARY                   \n";
    std::cout << "========================================================\n";
    std::cout << "Total Events Processed : " << summary.total_events << "\n";
    std::cout << "  - New Orders         : " << summary.new_orders << "\n";
    std::cout << "  - Cancel Orders      : " << summary.cancel_orders << "\n";
    std::cout << "  - Accepted Events    : " << summary.accepted_events << "\n";
    std::cout << "  - Rejected Events    : " << summary.rejected_events << "\n";
    std::cout << "Total Trades Executed  : " << recorder.trades().size() << "\n";
    std::cout << "Total Volume Traded    : " << summary.total_executed_volume << "\n";
    std::cout << "--------------------------------------------------------\n";
    std::cout << "Final Active Orders    : " << engine.book().size() << "\n";
    std::cout << "Final Best Bid         : " << (engine.book().best_bid().has_value() ? std::to_string(*engine.book().best_bid()) + " ticks" : "None") << "\n";
    std::cout << "Final Best Ask         : " << (engine.book().best_ask().has_value() ? std::to_string(*engine.book().best_ask()) + " ticks" : "None") << "\n";
    std::cout << "Total Resting Bid Vol  : " << engine.book().total_bid_volume() << "\n";
    std::cout << "Total Resting Ask Vol  : " << engine.book().total_ask_volume() << "\n";
    std::cout << "Engine Invariants      : " << (engine.check_invariants() ? "PASSED (OK)" : "FAILED (CORRUPTED)") << "\n";
    std::cout << "========================================================\n";

    // Export structured files
    std::string out_dir = "output";
    if (recorder.export_all(out_dir)) {
        std::cout << "Structured outputs exported successfully to directory: " << out_dir << "/\n";
        std::cout << "  - " << out_dir << "/trades.csv\n";
        std::cout << "  - " << out_dir << "/outcomes.csv\n";
        std::cout << "  - " << out_dir << "/depth_snapshots.csv\n";
        std::cout << "  - " << out_dir << "/run_summary.json\n";
    } else {
        std::cerr << "[Warning] Failed to export one or more output files to " << out_dir << "/\n";
    }

    return 0;
}