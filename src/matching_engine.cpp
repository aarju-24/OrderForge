#include "matching_engine.hpp"
#include <algorithm>

std::vector<Trade> MatchingEngine::process_order(Order incoming_order) {
    std::vector<Trade> trades;

    // 1. Validation Check
    if (incoming_order.initial_quantity == 0) {
        incoming_order.status = OrderStatus::Rejected;
        return trades;
    }

    incoming_order.remaining_quantity = incoming_order.initial_quantity;
    incoming_order.status = OrderStatus::New;

    // 2. Matching Loop
    if (incoming_order.side == Side::Buy) {
        // Incoming BUY order matches against ASKS (Sellers)
        while (incoming_order.remaining_quantity > 0 && book_.best_ask() != 0) {
            Price best_ask_price = book_.best_ask();

            // Check Price Crossing Condition for Limit Orders
            if (incoming_order.type == OrderType::Limit && incoming_order.price < best_ask_price) {
                break; // Incoming buy price is lower than best sell price -> Cannot match
            }

            // Get the order queue at the best ask level
            auto& ask_queue = book_.get_asks()[best_ask_price];
            auto it = ask_queue.begin();

            while (it != ask_queue.end() && incoming_order.remaining_quantity > 0) {
                Order& resting_ask = *it;

                Quantity trade_qty = std::min(incoming_order.remaining_quantity, resting_ask.remaining_quantity);

                // Update Quantities
                incoming_order.remaining_quantity -= trade_qty;
                resting_ask.remaining_quantity -= trade_qty;

                // Record Trade Execution (Execution occurs at resting order's price)
                trades.push_back(Trade{
                    incoming_order.order_id,  // buy_order_id
                    resting_ask.order_id,     // sell_order_id
                    best_ask_price,           // execution_price
                    trade_qty,                // executed_quantity
                    ++current_timestamp_      // timestamp
                });

                // Update Statuses & Clean Up Fully Filled Resting Orders
                if (resting_ask.remaining_quantity == 0) {
                    resting_ask.status = OrderStatus::Filled;
                    OrderId fill_id = resting_ask.order_id;
                    it = ask_queue.erase(it);
                    book_.get_order_index().erase(fill_id);
                } else {
                    resting_ask.status = OrderStatus::PartiallyFilled;
                    break; // Incoming order fully matched against this level
                }
            }

            // Clean up empty price level
            if (ask_queue.empty()) {
                book_.get_asks().erase(best_ask_price);
            }
        }
    } else {
        // Incoming SELL order matches against BIDS (Buyers)
        while (incoming_order.remaining_quantity > 0 && book_.best_bid() != 0) {
            Price best_bid_price = book_.best_bid();

            // Check Price Crossing Condition for Limit Orders
            if (incoming_order.type == OrderType::Limit && incoming_order.price > best_bid_price) {
                break; // Incoming sell price is higher than best buy price -> Cannot match
            }

            auto& bid_queue = book_.get_bids()[best_bid_price];
            auto it = bid_queue.begin();

            while (it != bid_queue.end() && incoming_order.remaining_quantity > 0) {
                Order& resting_bid = *it;

                Quantity trade_qty = std::min(incoming_order.remaining_quantity, resting_bid.remaining_quantity);

                incoming_order.remaining_quantity -= trade_qty;
                resting_bid.remaining_quantity -= trade_qty;

                trades.push_back(Trade{
                    resting_bid.order_id,     // buy_order_id
                    incoming_order.order_id,  // sell_order_id
                    best_bid_price,           // execution_price
                    trade_qty,                // executed_quantity
                    ++current_timestamp_      // timestamp
                });

                if (resting_bid.remaining_quantity == 0) {
                    resting_bid.status = OrderStatus::Filled;
                    OrderId fill_id = resting_bid.order_id;
                    it = bid_queue.erase(it);
                    book_.get_order_index().erase(fill_id);
                } else {
                    resting_bid.status = OrderStatus::PartiallyFilled;
                    break;
                }
            }

            if (bid_queue.empty()) {
                book_.get_bids().erase(best_bid_price);
            }
        }
    }

    // 3. Handle Remaining Quantity
    if (incoming_order.remaining_quantity == 0) {
        incoming_order.status = OrderStatus::Filled;
    } else if (incoming_order.remaining_quantity < incoming_order.initial_quantity) {
        incoming_order.status = OrderStatus::PartiallyFilled;
    }

    // If incoming LIMIT order has remainder, add to book
    if (incoming_order.remaining_quantity > 0 && incoming_order.type == OrderType::Limit) {
        book_.add_order(incoming_order);
    }

    return trades;
}