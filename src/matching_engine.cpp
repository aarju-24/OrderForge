#include "matching_engine.hpp"
#include <algorithm>

OrderResult MatchingEngine::process_order(Order incoming_order) {
    OrderResult result;
    result.order_id = incoming_order.order_id;
    result.order_type = incoming_order.type;
    result.side = incoming_order.side;
    result.price = incoming_order.price;
    result.initial_quantity = incoming_order.initial_quantity;
    result.executed_quantity = 0;
    result.remaining_quantity = 0;
    result.cancelled_quantity = 0;

    // 1. Validation Checks
    if (incoming_order.order_id == 0) {
        result.status = OrderStatus::Rejected;
        result.reject_reason = "Order ID must be positive";
        return result;
    }

    if (incoming_order.initial_quantity == 0) {
        result.status = OrderStatus::Rejected;
        result.reject_reason = "Order quantity must be positive";
        return result;
    }

    // Check duplicate active or historical order ID
    if (book_.has_order(incoming_order.order_id) || historical_orders_.find(incoming_order.order_id) != historical_orders_.end()) {
        result.status = OrderStatus::Rejected;
        result.reject_reason = "Duplicate order ID";
        return result;
    }

    // Limit order specific validation
    if (incoming_order.type == OrderType::Limit) {
        if (incoming_order.price <= 0) {
            result.status = OrderStatus::Rejected;
            result.reject_reason = "Limit price must be positive";
            return result;
        }
        if (tick_size_ > 0 && (incoming_order.price % tick_size_ != 0)) {
            result.status = OrderStatus::Rejected;
            result.reject_reason = "Price violates configured tick size";
            return result;
        }
    }

    incoming_order.remaining_quantity = incoming_order.initial_quantity;
    incoming_order.status = OrderStatus::New;

    // 2. Matching Loop
    if (incoming_order.side == Side::Buy) {
        while (incoming_order.remaining_quantity > 0 && book_.best_ask().has_value()) {
            Price best_ask_price = *book_.best_ask();

            // Price Crossing Condition for Limit Orders
            if (incoming_order.type == OrderType::Limit && incoming_order.price < best_ask_price) {
                break; // Buy limit price is below lowest ask -> Cannot match
            }

            const Order& resting_ask = book_.best_ask_order();
            Quantity trade_qty = std::min(incoming_order.remaining_quantity, resting_ask.remaining_quantity);

            Trade trade{
                ++trade_id_counter_,
                incoming_order.order_id,
                resting_ask.order_id,
                best_ask_price,
                trade_qty,
                ++current_timestamp_
            };

            result.trades.push_back(trade);
            trades_history_.push_back(trade);

            incoming_order.remaining_quantity -= trade_qty;
            result.executed_quantity += trade_qty;

            // If resting order is fully filled, archive it to historical orders
            if (trade_qty == resting_ask.remaining_quantity) {
                Order filled_order = resting_ask;
                filled_order.remaining_quantity = 0;
                filled_order.status = OrderStatus::Filled;
                historical_orders_[filled_order.order_id] = filled_order;
            }

            book_.fill_best_ask(trade_qty);
        }
    } else {
        while (incoming_order.remaining_quantity > 0 && book_.best_bid().has_value()) {
            Price best_bid_price = *book_.best_bid();

            // Price Crossing Condition for Limit Orders
            if (incoming_order.type == OrderType::Limit && incoming_order.price > best_bid_price) {
                break; // Sell limit price is above highest bid -> Cannot match
            }

            const Order& resting_bid = book_.best_bid_order();
            Quantity trade_qty = std::min(incoming_order.remaining_quantity, resting_bid.remaining_quantity);

            Trade trade{
                ++trade_id_counter_,
                resting_bid.order_id,
                incoming_order.order_id,
                best_bid_price,
                trade_qty,
                ++current_timestamp_
            };

            result.trades.push_back(trade);
            trades_history_.push_back(trade);

            incoming_order.remaining_quantity -= trade_qty;
            result.executed_quantity += trade_qty;

            if (trade_qty == resting_bid.remaining_quantity) {
                Order filled_order = resting_bid;
                filled_order.remaining_quantity = 0;
                filled_order.status = OrderStatus::Filled;
                historical_orders_[filled_order.order_id] = filled_order;
            }

            book_.fill_best_bid(trade_qty);
        }
    }

    // 3. Handle Remaining Quantity & Order Lifecycle Accounting
    if (incoming_order.remaining_quantity == 0) {
        incoming_order.status = OrderStatus::Filled;
        result.status = OrderStatus::Filled;
        result.remaining_quantity = 0;
        result.cancelled_quantity = 0;
        historical_orders_[incoming_order.order_id] = incoming_order;
    } else if (incoming_order.remaining_quantity < incoming_order.initial_quantity) {
        if (incoming_order.type == OrderType::Limit) {
            incoming_order.status = OrderStatus::PartiallyFilled;
            result.status = OrderStatus::PartiallyFilled;
            result.remaining_quantity = incoming_order.remaining_quantity;
            result.cancelled_quantity = 0;
            book_.add_order(incoming_order);
        } else {
            // Market order remainder is immediately cancelled
            result.status = OrderStatus::PartiallyFilled;
            result.remaining_quantity = 0;
            result.cancelled_quantity = incoming_order.remaining_quantity;
            incoming_order.status = OrderStatus::Cancelled;
            historical_orders_[incoming_order.order_id] = incoming_order;
        }
    } else {
        // Zero fills
        if (incoming_order.type == OrderType::Limit) {
            incoming_order.status = OrderStatus::New;
            result.status = OrderStatus::New;
            result.remaining_quantity = incoming_order.initial_quantity;
            result.cancelled_quantity = 0;
            book_.add_order(incoming_order);
        } else {
            // Unfilled market order remainder with no liquidity is cancelled
            result.status = OrderStatus::Cancelled;
            result.remaining_quantity = 0;
            result.cancelled_quantity = incoming_order.initial_quantity;
            incoming_order.status = OrderStatus::Cancelled;
            historical_orders_[incoming_order.order_id] = incoming_order;
        }
    }

    return result;
}

OrderResult MatchingEngine::cancel_order(OrderId order_id) {
    OrderResult result;
    result.order_id = order_id;

    if (order_id == 0) {
        result.status = OrderStatus::Rejected;
        result.reject_reason = "Order ID must be positive";
        return result;
    }

    auto cancelled_opt = book_.cancel_order(order_id);
    if (!cancelled_opt.has_value()) {
        result.status = OrderStatus::Rejected;
        if (historical_orders_.find(order_id) != historical_orders_.end()) {
            result.reject_reason = "Order is already filled or cancelled";
        } else {
            result.reject_reason = "Order ID not found";
        }
        return result;
    }

    const Order& cancelled_order = *cancelled_opt;
    result.status = OrderStatus::Cancelled;
    result.order_type = cancelled_order.type;
    result.side = cancelled_order.side;
    result.price = cancelled_order.price;
    result.initial_quantity = cancelled_order.initial_quantity;
    result.executed_quantity = cancelled_order.executed_quantity();
    result.remaining_quantity = 0;
    result.cancelled_quantity = cancelled_order.remaining_quantity;

    Order final_order = cancelled_order;
    final_order.remaining_quantity = 0;
    final_order.status = OrderStatus::Cancelled;
    historical_orders_[order_id] = final_order;

    return result;
}