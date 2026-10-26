#include "order_book.hpp"
#include <stdexcept>

bool OrderBook::add_order(const Order& order) {
    if (order.remaining_quantity == 0 || has_order(order.order_id)) {
        return false;
    }

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
    return true;
}

Optional<Order> OrderBook::cancel_order(OrderId order_id) {
    auto it = order_index_.find(order_id);
    if (it == order_index_.end()) {
        return NullOpt;
    }

    const auto loc = it->second;
    order_index_.erase(it);

    Order cancelled_order{};
    if (loc.side == Side::Buy) {
        auto price_it = bids_.find(loc.price);
        if (price_it != bids_.end()) {
            cancelled_order = *loc.iterator;
            price_it->second.erase(loc.iterator);
            if (price_it->second.empty()) {
                bids_.erase(price_it);
            }
        }
    } else {
        auto price_it = asks_.find(loc.price);
        if (price_it != asks_.end()) {
            cancelled_order = *loc.iterator;
            price_it->second.erase(loc.iterator);
            if (price_it->second.empty()) {
                asks_.erase(price_it);
            }
        }
    }

    return cancelled_order;
}

bool OrderBook::has_order(OrderId order_id) const {
    return order_index_.find(order_id) != order_index_.end();
}

Optional<Price> OrderBook::best_bid() const {
    if (bids_.empty()) return NullOpt;
    return bids_.begin()->first;
}

Optional<Price> OrderBook::best_ask() const {
    if (asks_.empty()) return NullOpt;
    return asks_.begin()->first;
}

const Order& OrderBook::best_ask_order() const {
    if (asks_.empty() || asks_.begin()->second.empty()) {
        throw std::runtime_error("OrderBook::best_ask_order() called on empty ask book");
    }
    return asks_.begin()->second.front();
}

const Order& OrderBook::best_bid_order() const {
    if (bids_.empty() || bids_.begin()->second.empty()) {
        throw std::runtime_error("OrderBook::best_bid_order() called on empty bid book");
    }
    return bids_.begin()->second.front();
}

void OrderBook::fill_best_ask(Quantity executed_qty) {
    if (asks_.empty()) return;

    auto price_it = asks_.begin();
    auto& queue = price_it->second;
    Order& resting_order = queue.front();

    if (executed_qty >= resting_order.remaining_quantity) {
        order_index_.erase(resting_order.order_id);
        queue.pop_front();
        if (queue.empty()) {
            asks_.erase(price_it);
        }
    } else {
        resting_order.remaining_quantity -= executed_qty;
        resting_order.status = OrderStatus::PartiallyFilled;
    }
}

void OrderBook::fill_best_bid(Quantity executed_qty) {
    if (bids_.empty()) return;

    auto price_it = bids_.begin();
    auto& queue = price_it->second;
    Order& resting_order = queue.front();

    if (executed_qty >= resting_order.remaining_quantity) {
        order_index_.erase(resting_order.order_id);
        queue.pop_front();
        if (queue.empty()) {
            bids_.erase(price_it);
        }
    } else {
        resting_order.remaining_quantity -= executed_qty;
        resting_order.status = OrderStatus::PartiallyFilled;
    }
}

Quantity OrderBook::total_bid_volume() const {
    Quantity total = 0;
    for (const auto& kv : bids_) {
        for (const auto& order : kv.second) {
            total += order.remaining_quantity;
        }
    }
    return total;
}

Quantity OrderBook::total_ask_volume() const {
    Quantity total = 0;
    for (const auto& kv : asks_) {
        for (const auto& order : kv.second) {
            total += order.remaining_quantity;
        }
    }
    return total;
}

Quantity OrderBook::volume_at_price(Side side, Price price) const {
    Quantity total = 0;
    if (side == Side::Buy) {
        auto it = bids_.find(price);
        if (it != bids_.end()) {
            for (const auto& order : it->second) {
                total += order.remaining_quantity;
            }
        }
    } else {
        auto it = asks_.find(price);
        if (it != asks_.end()) {
            for (const auto& order : it->second) {
                total += order.remaining_quantity;
            }
        }
    }
    return total;
}

std::vector<Level> OrderBook::get_bid_depth(size_t max_levels) const {
    std::vector<Level> depth;
    size_t count = 0;
    for (const auto& kv : bids_) {
        if (max_levels > 0 && count >= max_levels) break;
        Quantity total_qty = 0;
        for (const auto& order : kv.second) {
            total_qty += order.remaining_quantity;
        }
        depth.push_back(Level{kv.first, total_qty, kv.second.size()});
        ++count;
    }
    return depth;
}

std::vector<Level> OrderBook::get_ask_depth(size_t max_levels) const {
    std::vector<Level> depth;
    size_t count = 0;
    for (const auto& kv : asks_) {
        if (max_levels > 0 && count >= max_levels) break;
        Quantity total_qty = 0;
        for (const auto& order : kv.second) {
            total_qty += order.remaining_quantity;
        }
        depth.push_back(Level{kv.first, total_qty, kv.second.size()});
        ++count;
    }
    return depth;
}

bool OrderBook::check_invariants() const {
    // Invariant 1: Uncrossed book when both sides are present
    if (has_bids() && has_asks()) {
        if (*best_bid() >= *best_ask()) {
            return false; // Crossed book detected!
        }
    }

    // Invariant 2: Total orders in all price queues must equal order_index_ size
    size_t total_orders = 0;
    for (const auto& kv : bids_) {
        if (kv.second.empty()) return false; // Invariant 3: No empty price queues
        for (const auto& order : kv.second) {
            if (order.remaining_quantity == 0) return false; // Invariant 4: Positive remaining qty
            if (order.price != kv.first) return false;       // Price matches queue key
            if (order.side != Side::Buy) return false;       // Side matches
            ++total_orders;
        }
    }

    for (const auto& kv : asks_) {
        if (kv.second.empty()) return false;
        for (const auto& order : kv.second) {
            if (order.remaining_quantity == 0) return false;
            if (order.price != kv.first) return false;
            if (order.side != Side::Sell) return false;
            ++total_orders;
        }
    }

    if (order_index_.size() != total_orders) {
        return false; // Index size mismatch
    }

    // Invariant 5: Every index entry points to the exact active order
    for (const auto& kv : order_index_) {
        OrderId id = kv.first;
        const auto& loc = kv.second;
        if (loc.side == Side::Buy) {
            auto price_it = bids_.find(loc.price);
            if (price_it == bids_.end()) return false;
            if (loc.iterator == price_it->second.end()) return false;
            if (loc.iterator->order_id != id) return false;
            if (loc.iterator->price != loc.price) return false;
        } else {
            auto price_it = asks_.find(loc.price);
            if (price_it == asks_.end()) return false;
            if (loc.iterator == price_it->second.end()) return false;
            if (loc.iterator->order_id != id) return false;
            if (loc.iterator->price != loc.price) return false;
        }
    }

    return true;
}