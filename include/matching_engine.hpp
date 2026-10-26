#ifndef MATCHING_ENGINE_HPP
#define MATCHING_ENGINE_HPP

#include <vector>
#include <unordered_map>
#include "order_book.hpp"
#include "trade.hpp"
#include "types.hpp"
#include "order.hpp"
#include "order_result.hpp"

class MatchingEngine {
public:
    explicit MatchingEngine(Price tick_size = 1) : tick_size_(tick_size) {}

    // Process an incoming order and return structured execution report
    OrderResult process_order(Order incoming_order);

    // Cancel an order by ID
    OrderResult cancel_order(OrderId order_id);

    // Queries
    const OrderBook& book() const { return book_; }
    const std::vector<Trade>& trade_history() const { return trades_history_; }
    Price tick_size() const { return tick_size_; }
    void set_tick_size(Price tick_size) { tick_size_ = tick_size; }

    Optional<Order> get_order_history(OrderId order_id) const {
        auto it = historical_orders_.find(order_id);
        if (it != historical_orders_.end()) {
            return it->second;
        }
        return NullOpt;
    }

    // Depth queries pass-through
    std::vector<Level> get_bid_depth(size_t max_levels = 0) const { return book_.get_bid_depth(max_levels); }
    std::vector<Level> get_ask_depth(size_t max_levels = 0) const { return book_.get_ask_depth(max_levels); }

    TradeId last_trade_id() const { return trade_id_counter_; }

    // Engine invariant verification
    bool check_invariants() const {
        if (!book_.check_invariants()) return false;
        for (const auto& trade : trades_history_) {
            if (trade.executed_quantity == 0) return false;
            if (trade.execution_price <= 0) return false;
        }
        return true;
    }


private:
    OrderBook book_;
    Price tick_size_{1};
    Timestamp current_timestamp_{10000};
    TradeId trade_id_counter_{0};
    std::vector<Trade> trades_history_;
    std::unordered_map<OrderId, Order> historical_orders_;
};

#endif // MATCHING_ENGINE_HPP