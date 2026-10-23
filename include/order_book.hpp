#ifndef ORDER_BOOK_HPP
#define ORDER_BOOK_HPP

#include <map>
#include <list>
#include <unordered_map>
#include <functional>
#include "types.hpp"
#include "order.hpp"

class OrderBook {
public:
    using OrderList = std::list<Order>;

    void add_order(const Order& order);
    bool cancel_order(OrderId order_id);
    Price best_bid() const;
    Price best_ask() const;
    size_t size() const { return order_index_.size(); }

    // Direct accessors for MatchingEngine sweep logic
    auto& get_bids() { return bids_; }
    auto& get_asks() { return asks_; }
    auto& get_order_index() { return order_index_; }

private:
    struct OrderLocation {
        Price price;
        Side side;
        OrderList::iterator iterator;
    };

    std::map<Price, OrderList, std::greater<Price>> bids_;
    std::map<Price, OrderList, std::less<Price>> asks_;
    std::unordered_map<OrderId, OrderLocation> order_index_;
};

#endif // ORDER_BOOK_HPP