#include "order_book.hpp"

void OrderBook::add_order(const Order& order) {
    if (order.side == Side::Buy) {
        auto& queue = bids_[order.price];
        queue.push_back(order);
        auto it = std::prev(queue.end());
        order_index_[order.order_id] = {order.price, Side::Buy, it};
    } else {
        auto& queue = asks_[order.price];
        queue.push_back(order);
        auto it = std::prev(queue.end());
        order_index_[order.order_id] = {order.price, Side::Sell, it};
    }
}

bool OrderBook::cancel_order(OrderId order_id) {
    auto it = order_index_.find(order_id);
    if (it == order_index_.end()) {
        return false;
    }

    const auto& loc = it->second;
    if (loc.side == Side::Buy) {
        auto price_it = bids_.find(loc.price);
        if (price_it != bids_.end()) {
            price_it->second.erase(loc.iterator);
            if (price_it->second.empty()) {
                bids_.erase(price_it);
            }
        }
    } else {
        auto price_it = asks_.find(loc.price);
        if (price_it != asks_.end()) {
            price_it->second.erase(loc.iterator);
            if (price_it->second.empty()) {
                asks_.erase(price_it);
            }
        }
    }

    order_index_.erase(it);
    return true;
}

Price OrderBook::best_bid() const {
    if (bids_.empty()) return 0;
    return bids_.begin()->first;
}

Price OrderBook::best_ask() const {
    if (asks_.empty()) return 0;
    return asks_.begin()->first;
}