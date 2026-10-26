#include <gtest/gtest.h>
#include <vector>
#include <algorithm>
#include <random>
#include "matching_engine.hpp"

// -------------------------------------------------------------------------
// NaiveReferenceMatcher: An independent, vector-based reference matcher
// with strictly separated buy and sell arrays for differential testing.
// -------------------------------------------------------------------------
struct NaiveOrder {
    OrderId id;
    Side side;
    OrderType type;
    Price price;
    Quantity initial_quantity;
    Quantity remaining_quantity;
    uint64_t arrival_index;
};

struct NaiveTrade {
    OrderId buy_order_id;
    OrderId sell_order_id;
    Price execution_price;
    Quantity executed_quantity;
};

class NaiveReferenceMatcher {
public:
    std::vector<NaiveTrade> process_order(const Order& order) {
        std::vector<NaiveTrade> trades;
        if (order.order_id == 0 || order.initial_quantity == 0) return trades;
        if (order.type == OrderType::Limit && order.price <= 0) return trades;

        // Check duplicate
        for (const auto& resting : resting_buys_) {
            if (resting.id == order.order_id) return trades;
        }
        for (const auto& resting : resting_sells_) {
            if (resting.id == order.order_id) return trades;
        }

        NaiveOrder incoming{order.order_id, order.side, order.type, order.price, 
                            order.initial_quantity, order.initial_quantity, ++order_counter_};

        if (incoming.side == Side::Buy) {
            // Sort resting sells: lowest price first, then earliest arrival (FIFO)
            std::sort(resting_sells_.begin(), resting_sells_.end(), [](const NaiveOrder& a, const NaiveOrder& b) {
                if (a.price != b.price) return a.price < b.price;
                return a.arrival_index < b.arrival_index;
            });

            auto it = resting_sells_.begin();
            while (it != resting_sells_.end() && incoming.remaining_quantity > 0) {
                if (incoming.type == OrderType::Limit && incoming.price < it->price) {
                    break;
                }

                Quantity trade_qty = std::min(incoming.remaining_quantity, it->remaining_quantity);
                trades.push_back(NaiveTrade{incoming.id, it->id, it->price, trade_qty});

                incoming.remaining_quantity -= trade_qty;
                it->remaining_quantity -= trade_qty;

                if (it->remaining_quantity == 0) {
                    it = resting_sells_.erase(it);
                } else {
                    break;
                }
            }

            if (incoming.remaining_quantity > 0 && incoming.type == OrderType::Limit) {
                resting_buys_.push_back(incoming);
            }
        } else {
            // Sort resting buys: highest price first, then earliest arrival (FIFO)
            std::sort(resting_buys_.begin(), resting_buys_.end(), [](const NaiveOrder& a, const NaiveOrder& b) {
                if (a.price != b.price) return a.price > b.price;
                return a.arrival_index < b.arrival_index;
            });

            auto it = resting_buys_.begin();
            while (it != resting_buys_.end() && incoming.remaining_quantity > 0) {
                if (incoming.type == OrderType::Limit && incoming.price > it->price) {
                    break;
                }

                Quantity trade_qty = std::min(incoming.remaining_quantity, it->remaining_quantity);
                trades.push_back(NaiveTrade{it->id, incoming.id, it->price, trade_qty});

                incoming.remaining_quantity -= trade_qty;
                it->remaining_quantity -= trade_qty;

                if (it->remaining_quantity == 0) {
                    it = resting_buys_.erase(it);
                } else {
                    break;
                }
            }

            if (incoming.remaining_quantity > 0 && incoming.type == OrderType::Limit) {
                resting_sells_.push_back(incoming);
            }
        }

        return trades;
    }

    bool cancel_order(OrderId order_id) {
        for (auto it = resting_buys_.begin(); it != resting_buys_.end(); ++it) {
            if (it->id == order_id) {
                resting_buys_.erase(it);
                return true;
            }
        }
        for (auto it = resting_sells_.begin(); it != resting_sells_.end(); ++it) {
            if (it->id == order_id) {
                resting_sells_.erase(it);
                return true;
            }
        }
        return false;
    }

    size_t size() const { return resting_buys_.size() + resting_sells_.size(); }

private:
    std::vector<NaiveOrder> resting_buys_;
    std::vector<NaiveOrder> resting_sells_;
    uint64_t order_counter_{0};
};

// -------------------------------------------------------------------------
// Differential Random Fuzzing Test (Seed 1337)
// -------------------------------------------------------------------------
TEST(DifferentialTest, RandomizedStressAgainstNaiveMatcher) {
    MatchingEngine engine;
    NaiveReferenceMatcher naive;

    std::mt19937_64 rng(1337);
    std::uniform_int_distribution<int> op_dist(1, 10);
    std::uniform_int_distribution<int> side_dist(0, 1);
    std::uniform_int_distribution<int> type_dist(1, 10);
    std::uniform_int_distribution<int64_t> price_dist(9950, 10050);
    std::uniform_int_distribution<uint32_t> qty_dist(10, 100);

    std::vector<OrderId> active_order_ids;
    OrderId next_id = 1;

    const size_t total_operations = 1000;

    for (size_t step = 0; step < total_operations; ++step) {
        int op = op_dist(rng);

        if (op <= 8 || active_order_ids.empty()) {
            // New Order (80% probability)
            OrderId id = next_id++;
            Side side = (side_dist(rng) == 0) ? Side::Buy : Side::Sell;
            OrderType type = (type_dist(rng) <= 2) ? OrderType::Market : OrderType::Limit;
            Price price = (type == OrderType::Market) ? 0 : price_dist(rng);
            Quantity qty = qty_dist(rng);

            Order order{id, side, type, price, qty, qty, OrderStatus::New};

            auto eng_res = engine.process_order(order);
            auto naive_trades = naive.process_order(order);

            // 1. Verify accounting
            ASSERT_TRUE(eng_res.verify_accounting()) << "Accounting violated on order " << id;

            // 2. Compare trades
            ASSERT_EQ(eng_res.trades.size(), naive_trades.size()) 
                << "Trade count mismatch on order " << id << " at step " << step;

            for (size_t i = 0; i < eng_res.trades.size(); ++i) {
                EXPECT_EQ(eng_res.trades[i].buy_order_id, naive_trades[i].buy_order_id);
                EXPECT_EQ(eng_res.trades[i].sell_order_id, naive_trades[i].sell_order_id);
                EXPECT_EQ(eng_res.trades[i].execution_price, naive_trades[i].execution_price);
                EXPECT_EQ(eng_res.trades[i].executed_quantity, naive_trades[i].executed_quantity);
            }

            if (eng_res.remaining_quantity > 0 && type == OrderType::Limit) {
                active_order_ids.push_back(id);
            }
        } else {
            // Cancel Order (20% probability)
            std::uniform_int_distribution<size_t> idx_dist(0, active_order_ids.size() - 1);
            size_t idx = idx_dist(rng);
            OrderId cancel_id = active_order_ids[idx];

            auto cancel_res = engine.cancel_order(cancel_id);
            bool naive_cancelled = naive.cancel_order(cancel_id);

            EXPECT_EQ(cancel_res.is_accepted(), naive_cancelled) 
                << "Cancel disagreement on ID " << cancel_id;

            active_order_ids.erase(active_order_ids.begin() + idx);
        }

        // 3. Verify structural and market invariants after every step
        ASSERT_TRUE(engine.check_invariants()) 
            << "Invariants broken at step " << step;
        ASSERT_EQ(engine.book().size(), naive.size()) 
            << "Book size mismatch at step " << step;
    }
}
