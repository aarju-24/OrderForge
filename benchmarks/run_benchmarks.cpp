#include <iostream>
#include <iomanip>
#include <vector>
#include <chrono>
#include <algorithm>
#include <numeric>
#include <fstream>
#include <random>
#include <sstream>
#include "matching_engine.hpp"
#include "event_reader.hpp"

#ifdef _WIN32
#include <direct.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define MKDIR(dir) _mkdir(dir)

struct PrecisionTimer {
    static int64_t now() {
        LARGE_INTEGER t;
        QueryPerformanceCounter(&t);
        return t.QuadPart;
    }
    static double to_us(int64_t ticks) {
        static LARGE_INTEGER freq = [](){
            LARGE_INTEGER f;
            QueryPerformanceFrequency(&f);
            return f;
        }();
        return (ticks * 1000000.0) / freq.QuadPart;
    }
    static double to_ms(int64_t ticks) {
        static LARGE_INTEGER freq = [](){
            LARGE_INTEGER f;
            QueryPerformanceFrequency(&f);
            return f;
        }();
        return (ticks * 1000.0) / freq.QuadPart;
    }
};
#else
#include <sys/stat.h>
#define MKDIR(dir) mkdir(dir, 0755)

struct PrecisionTimer {
    static int64_t now() {
        return std::chrono::high_resolution_clock::now().time_since_epoch().count();
    }
    static double to_us(int64_t ticks) {
        return ticks / 1000.0;
    }
    static double to_ms(int64_t ticks) {
        return ticks / 1000000.0;
    }
};
#endif

// Structure holding the results of a benchmark run
struct BenchmarkResult {
    std::string workload_name;
    size_t event_count{0};
    double duration_ms{0.0};
    double throughput_ops_sec{0.0};
    double mean_latency_us{0.0};
    double p50_latency_us{0.0};
    double p90_latency_us{0.0};
    double p95_latency_us{0.0};
    double p99_latency_us{0.0};
    double p999_latency_us{0.0};
    double max_latency_us{0.0};
    size_t trades_generated{0};
    bool invariants_passed{false};
};

// Workload Generator
class WorkloadGenerator {
public:
    // 1. Resting-heavy: passive orders that do not cross
    static std::vector<Event> resting_heavy(size_t count, uint32_t seed = 1001) {
        std::mt19937_64 rng(seed);
        std::uniform_int_distribution<Price> bid_price_dist(10000, 14990);
        std::uniform_int_distribution<Price> ask_price_dist(15010, 20000);
        std::uniform_int_distribution<Quantity> qty_dist(10, 100);

        std::vector<Event> events;
        events.reserve(count);

        for (size_t i = 1; i <= count; ++i) {
            Side side = (i % 2 == 1) ? Side::Buy : Side::Sell;
            Price price = (side == Side::Buy) ? bid_price_dist(rng) : ask_price_dist(rng);
            Quantity qty = qty_dist(rng);
            Order ord{i, side, OrderType::Limit, price, qty, qty, OrderStatus::New};
            events.push_back(Event{i, EventType::NewOrder, ord, i});
        }
        return events;
    }

    // 2. Cancellation-heavy: Rapid insert followed by cancel (50% churn)
    static std::vector<Event> cancellation_heavy(size_t count, uint32_t seed = 2002) {
        std::mt19937_64 rng(seed);
        std::uniform_int_distribution<Price> price_dist(14000, 16000);
        std::uniform_int_distribution<Quantity> qty_dist(10, 100);

        std::vector<Event> events;
        events.reserve(count);

        OrderId id = 1;
        while (events.size() < count) {
            // New Order
            OrderId cur_id = id++;
            Price price = price_dist(rng);
            Quantity qty = qty_dist(rng);
            Order ord{cur_id, Side::Buy, OrderType::Limit, price, qty, qty, OrderStatus::New};
            events.push_back(Event{static_cast<uint64_t>(events.size() + 1), EventType::NewOrder, ord, events.size() + 1});

            if (events.size() < count) {
                // Immediate Cancel
                Order cancel_ord{};
                cancel_ord.order_id = cur_id;
                events.push_back(Event{static_cast<uint64_t>(events.size() + 1), EventType::CancelOrder, cancel_ord, events.size() + 1});
            }
        }
        return events;
    }

    // 3. Match-heavy: Seed resting book, then fire crossing orders
    static std::vector<Event> match_heavy(size_t count, uint32_t seed = 3003) {
        std::mt19937_64 rng(seed);
        std::uniform_int_distribution<Quantity> qty_dist(10, 50);

        std::vector<Event> events;
        events.reserve(count);

        size_t half = count / 2;
        // Seed resting asks
        for (size_t i = 1; i <= half; ++i) {
            Price price = 15000 + (i % 50) * 10;
            Quantity qty = qty_dist(rng);
            Order ord{i, Side::Sell, OrderType::Limit, price, qty, qty, OrderStatus::New};
            events.push_back(Event{i, EventType::NewOrder, ord, i});
        }
        // Crossing aggressive bids
        for (size_t i = half + 1; i <= count; ++i) {
            Price price = 15500; // Above resting asks -> matches immediately
            Quantity qty = qty_dist(rng);
            Order ord{i, Side::Buy, OrderType::Limit, price, qty, qty, OrderStatus::New};
            events.push_back(Event{i, EventType::NewOrder, ord, i});
        }
        return events;
    }

    // 4. Deep-book ladder: Spread across 1,000 distinct price levels
    static std::vector<Event> deep_book_ladder(size_t count, uint32_t seed = 4004) {
        std::mt19937_64 rng(seed);
        std::uniform_int_distribution<Price> level_dist(1, 1000);
        std::uniform_int_distribution<Quantity> qty_dist(10, 100);

        std::vector<Event> events;
        events.reserve(count);

        for (size_t i = 1; i <= count; ++i) {
            Side side = (i % 2 == 1) ? Side::Buy : Side::Sell;
            Price offset = level_dist(rng) * 10;
            Price price = (side == Side::Buy) ? (10000 - offset) : (20000 + offset);
            Quantity qty = qty_dist(rng);
            Order ord{i, side, OrderType::Limit, price, qty, qty, OrderStatus::New};
            events.push_back(Event{i, EventType::NewOrder, ord, i});
        }
        return events;
    }

    // 5. Many orders at same price: Deep FIFO queue at single price level
    static std::vector<Event> many_orders_same_price(size_t count) {
        std::vector<Event> events;
        events.reserve(count);

        size_t half = count / 2;
        // Half 1: 50,000 asks sitting at 15000
        for (size_t i = 1; i <= half; ++i) {
            Order ord{i, Side::Sell, OrderType::Limit, 15000, 10, 10, OrderStatus::New};
            events.push_back(Event{i, EventType::NewOrder, ord, i});
        }
        // Half 2: 50,000 bids matching FIFO queue
        for (size_t i = half + 1; i <= count; ++i) {
            Order ord{i, Side::Buy, OrderType::Limit, 15000, 10, 10, OrderStatus::New};
            events.push_back(Event{i, EventType::NewOrder, ord, i});
        }
        return events;
    }

    // 6. Multi-level sweeps: Liquidity taker orders sweeping multiple price levels
    static std::vector<Event> multi_level_sweeps(size_t count, uint32_t seed = 6006) {
        std::mt19937_64 rng(seed);
        std::vector<Event> events;
        events.reserve(count);

        OrderId id = 1;
        while (events.size() < count) {
            // Seed 20 price levels of asks
            for (Price p = 15010; p <= 15200 && events.size() < count; p += 10) {
                OrderId cur_id = id++;
                Order ord{cur_id, Side::Sell, OrderType::Limit, p, 100, 100, OrderStatus::New};
                events.push_back(Event{cur_id, EventType::NewOrder, ord, events.size() + 1});
            }
            if (events.size() < count) {
                // Large aggressive buy sweeping 15 levels (1,500 shares)
                OrderId cur_id = id++;
                Order ord{cur_id, Side::Buy, OrderType::Limit, 15150, 1500, 1500, OrderStatus::New};
                events.push_back(Event{cur_id, EventType::NewOrder, ord, events.size() + 1});
            }
        }
        return events;
    }

    // 7. Mixed realistic synthetic flow: 60% passive, 25% crossing, 15% cancel
    static std::vector<Event> mixed_realistic(size_t count, uint32_t seed = 7007) {
        std::mt19937_64 rng(seed);
        std::uniform_int_distribution<int> type_dist(0, 99);
        std::uniform_int_distribution<int> side_dist(0, 1);
        std::uniform_int_distribution<Price> passive_bid(14000, 14990);
        std::uniform_int_distribution<Price> passive_ask(15010, 16000);
        std::uniform_int_distribution<Price> crossing_price(14950, 15050);
        std::uniform_int_distribution<Quantity> qty_dist(10, 100);

        std::vector<Event> events;
        events.reserve(count);

        OrderId next_id = 1;
        std::vector<OrderId> active_ids;

        for (size_t i = 0; i < count; ++i) {
            int roll = type_dist(rng);
            if (roll < 60) {
                // 60% Passive Limit
                OrderId id = next_id++;
                Side side = (side_dist(rng) == 0) ? Side::Buy : Side::Sell;
                Price price = (side == Side::Buy) ? (passive_bid(rng) / 10 * 10) : (passive_ask(rng) / 10 * 10);
                Quantity qty = qty_dist(rng);
                Order ord{id, side, OrderType::Limit, price, qty, qty, OrderStatus::New};
                events.push_back(Event{id, EventType::NewOrder, ord, events.size() + 1});
                active_ids.push_back(id);
            } else if (roll < 85 || active_ids.empty()) {
                // 25% Crossing Aggressive Limit
                OrderId id = next_id++;
                Side side = (side_dist(rng) == 0) ? Side::Buy : Side::Sell;
                Price price = crossing_price(rng) / 10 * 10;
                Quantity qty = qty_dist(rng);
                Order ord{id, side, OrderType::Limit, price, qty, qty, OrderStatus::New};
                events.push_back(Event{id, EventType::NewOrder, ord, events.size() + 1});
                active_ids.push_back(id);
            } else {
                // 15% Cancel
                std::uniform_int_distribution<size_t> idx_dist(0, active_ids.size() - 1);
                size_t idx = idx_dist(rng);
                OrderId target_id = active_ids[idx];
                active_ids.erase(active_ids.begin() + idx);

                Order cancel_ord{};
                cancel_ord.order_id = target_id;
                events.push_back(Event{target_id, EventType::CancelOrder, cancel_ord, events.size() + 1});
            }
        }
        return events;
    }
};

// Benchmark Harness
class BenchmarkHarness {
public:
    static BenchmarkResult run_workload(const std::string& name, const std::vector<Event>& events) {
        BenchmarkResult res;
        res.workload_name = name;
        res.event_count = events.size();

        // Pass 1: Pure Throughput (Engine-only, zero per-event timing overhead)
        {
            MatchingEngine engine;
            int64_t t0 = PrecisionTimer::now();
            for (const auto& ev : events) {
                if (ev.type == EventType::NewOrder) {
                    engine.process_order(ev.order);
                } else {
                    engine.cancel_order(ev.order.order_id);
                }
            }
            int64_t t1 = PrecisionTimer::now();
            res.duration_ms = PrecisionTimer::to_ms(t1 - t0);
            res.throughput_ops_sec = (res.event_count / res.duration_ms) * 1000.0;
            res.trades_generated = engine.trade_history().size();
            res.invariants_passed = engine.check_invariants();
        }

        // Pass 2: Latency Profiling (Per-event latency in microseconds)
        {
            MatchingEngine lat_engine;
            std::vector<double> latencies_us;
            latencies_us.reserve(events.size());

            for (const auto& ev : events) {
                int64_t start = PrecisionTimer::now();
                if (ev.type == EventType::NewOrder) {
                    lat_engine.process_order(ev.order);
                } else {
                    lat_engine.cancel_order(ev.order.order_id);
                }
                int64_t end = PrecisionTimer::now();
                double us = PrecisionTimer::to_us(end - start);
                latencies_us.push_back(us);
            }

            std::sort(latencies_us.begin(), latencies_us.end());

            double sum = std::accumulate(latencies_us.begin(), latencies_us.end(), 0.0);
            res.mean_latency_us = sum / latencies_us.size();
            res.p50_latency_us = latencies_us[static_cast<size_t>(latencies_us.size() * 0.50)];
            res.p90_latency_us = latencies_us[static_cast<size_t>(latencies_us.size() * 0.90)];
            res.p95_latency_us = latencies_us[static_cast<size_t>(latencies_us.size() * 0.95)];
            res.p99_latency_us = latencies_us[static_cast<size_t>(latencies_us.size() * 0.99)];
            res.p999_latency_us = latencies_us[static_cast<size_t>(latencies_us.size() * 0.999)];
            res.max_latency_us = latencies_us.back();
        }

        return res;
    }

    static void print_table(const std::vector<BenchmarkResult>& results) {
        std::cout << "\n========================================================================================================================\n";
        std::cout << "                                         ORDERFORGE PERFORMANCE BENCHMARK SUITE                                         \n";
        std::cout << "========================================================================================================================\n";
        std::cout << std::left 
                  << std::setw(26) << "Workload"
                  << std::setw(10) << "Events"
                  << std::setw(15) << "Throughput (ops/s)"
                  << std::setw(11) << "p50 (us)"
                  << std::setw(11) << "p95 (us)"
                  << std::setw(11) << "p99 (us)"
                  << std::setw(11) << "Max (us)"
                  << std::setw(10) << "Trades"
                  << std::setw(10) << "Invariants\n";
        std::cout << "------------------------------------------------------------------------------------------------------------------------\n";

        for (const auto& r : results) {
            std::cout << std::left 
                      << std::setw(26) << r.workload_name
                      << std::setw(10) << r.event_count
                      << std::setw(15) << static_cast<uint64_t>(r.throughput_ops_sec)
                      << std::setw(11) << std::fixed << std::setprecision(2) << r.p50_latency_us
                      << std::setw(11) << std::fixed << std::setprecision(2) << r.p95_latency_us
                      << std::setw(11) << std::fixed << std::setprecision(2) << r.p99_latency_us
                      << std::setw(11) << std::fixed << std::setprecision(2) << r.max_latency_us
                      << std::setw(10) << r.trades_generated
                      << std::setw(10) << (r.invariants_passed ? "PASS" : "FAIL") << "\n";
        }
        std::cout << "========================================================================================================================\n";
    }

    static bool export_csv(const std::string& path, const std::vector<BenchmarkResult>& results) {
        std::ofstream out(path);
        if (!out.is_open()) return false;

        out << "workload,event_count,duration_ms,throughput_ops_sec,mean_lat_us,p50_lat_us,p90_lat_us,p95_lat_us,p99_lat_us,p999_lat_us,max_lat_us,trades_generated,invariants_passed\n";
        for (const auto& r : results) {
            out << r.workload_name << ","
                << r.event_count << ","
                << r.duration_ms << ","
                << r.throughput_ops_sec << ","
                << r.mean_latency_us << ","
                << r.p50_latency_us << ","
                << r.p90_latency_us << ","
                << r.p95_latency_us << ","
                << r.p99_latency_us << ","
                << r.p999_latency_us << ","
                << r.max_latency_us << ","
                << r.trades_generated << ","
                << (r.invariants_passed ? "true" : "false") << "\n";
        }
        return true;
    }

    static bool export_json(const std::string& path, const std::vector<BenchmarkResult>& results) {
        std::ofstream out(path);
        if (!out.is_open()) return false;

        out << "{\n";
        out << "  \"results\": [\n";
        for (size_t i = 0; i < results.size(); ++i) {
            const auto& r = results[i];
            out << "    {\n";
            out << "      \"workload\": \"" << r.workload_name << "\",\n";
            out << "      \"event_count\": " << r.event_count << ",\n";
            out << "      \"duration_ms\": " << r.duration_ms << ",\n";
            out << "      \"throughput_ops_sec\": " << r.throughput_ops_sec << ",\n";
            out << "      \"mean_latency_us\": " << r.mean_latency_us << ",\n";
            out << "      \"p50_latency_us\": " << r.p50_latency_us << ",\n";
            out << "      \"p90_latency_us\": " << r.p90_latency_us << ",\n";
            out << "      \"p95_latency_us\": " << r.p95_latency_us << ",\n";
            out << "      \"p99_latency_us\": " << r.p99_latency_us << ",\n";
            out << "      \"p999_latency_us\": " << r.p999_latency_us << ",\n";
            out << "      \"max_latency_us\": " << r.max_latency_us << ",\n";
            out << "      \"trades_generated\": " << r.trades_generated << ",\n";
            out << "      \"invariants_passed\": " << (r.invariants_passed ? "true" : "false") << "\n";
            out << "    }" << (i + 1 < results.size() ? "," : "") << "\n";
        }
        out << "  ]\n";
        out << "}\n";
        return true;
    }
};

int main(int argc, char* argv[]) {
    size_t scale = 100000;
    if (argc > 1) {
        scale = std::stoul(argv[1]);
    }

    std::cout << "Initializing OrderForge Benchmark Harness (Workload Scale: " << scale << " events)...\n";

    std::vector<BenchmarkResult> results;

    std::cout << "1/9  Running Resting-Heavy Benchmark...\n";
    results.push_back(BenchmarkHarness::run_workload("Resting-Heavy", WorkloadGenerator::resting_heavy(scale)));

    std::cout << "2/9  Running Cancellation-Heavy Benchmark...\n";
    results.push_back(BenchmarkHarness::run_workload("Cancellation-Heavy", WorkloadGenerator::cancellation_heavy(scale)));

    std::cout << "3/9  Running Match-Heavy Benchmark...\n";
    results.push_back(BenchmarkHarness::run_workload("Match-Heavy", WorkloadGenerator::match_heavy(scale)));

    std::cout << "4/9  Running Deep-Book Ladder Benchmark (1,000 levels)...\n";
    results.push_back(BenchmarkHarness::run_workload("Deep-Book Ladder", WorkloadGenerator::deep_book_ladder(scale)));

    std::cout << "5/9  Running Many-At-Same-Price Benchmark...\n";
    results.push_back(BenchmarkHarness::run_workload("Many-At-Same-Price", WorkloadGenerator::many_orders_same_price(scale)));

    std::cout << "6/9  Running Multi-Level Sweeps Benchmark...\n";
    results.push_back(BenchmarkHarness::run_workload("Multi-Level Sweeps", WorkloadGenerator::multi_level_sweeps(scale)));

    std::cout << "7/9  Running Mixed-Realistic Benchmark (" << scale << " events)...\n";
    results.push_back(BenchmarkHarness::run_workload("Mixed-Realistic-100K", WorkloadGenerator::mixed_realistic(scale)));

    // Scaling benchmarks across input sizes
    std::cout << "8/9  Running Mixed-Realistic Scaling (10,000 events)...\n";
    results.push_back(BenchmarkHarness::run_workload("Mixed-Scale-10K", WorkloadGenerator::mixed_realistic(10000)));

    std::cout << "9/9  Running Mixed-Realistic Scaling (50,000 events)...\n";
    results.push_back(BenchmarkHarness::run_workload("Mixed-Scale-50K", WorkloadGenerator::mixed_realistic(50000)));

    BenchmarkHarness::print_table(results);

    MKDIR("output");
    BenchmarkHarness::export_csv("output/benchmark_results.csv", results);
    BenchmarkHarness::export_json("output/benchmark_results.json", results);

    std::cout << "Benchmark results exported successfully to:\n";
    std::cout << "  - output/benchmark_results.csv\n";
    std::cout << "  - output/benchmark_results.json\n";

    return 0;
}
