#ifndef ORDER_BOOK_HPP
#define ORDER_BOOK_HPP

#include <map>
#include <list>
#include <vector>
#include <unordered_map>
#include <functional>
#include "types.hpp"
#include "order.hpp"

// Level 2 aggregated market depth structure
struct Level {
    Price price{0};
    Quantity total_quantity{0};
    size_t order_count{0};

    bool operator==(const Level& other) const {
        return price == other.price && total_quantity == other.total_quantity && order_count == other.order_count;
    }
};

class OrderBook {
public:
    using OrderList = std::list<Order>;

    // Inserts an order into the book.
    // Returns true on success, false if duplicate ID or invalid quantity.
    bool add_order(const Order& order);

    // Cancels an active order by ID.
    // Returns the cancelled order if found and removed, NullOpt otherwise.
    Optional<Order> cancel_order(OrderId order_id);

    // State queries
    bool has_order(OrderId order_id) const;
    Optional<Price> best_bid() const;
    Optional<Price> best_ask() const;
    bool has_bids() const { return !bids_.empty(); }
    bool has_asks() const { return !asks_.empty(); }
    size_t size() const { return order_index_.size(); }

    // Depth aggregation
    Quantity total_bid_volume() const;
    Quantity total_ask_volume() const;
    Quantity volume_at_price(Side side, Price price) const;
    std::vector<Level> get_bid_depth(size_t max_levels = 0) const;
    std::vector<Level> get_ask_depth(size_t max_levels = 0) const;

    // Structural and market invariant verification
    bool check_invariants() const;

    // Matching sweep interfaces (encapsulated)
    const Order& best_ask_order() const;
    const Order& best_bid_order() const;
    void fill_best_ask(Quantity executed_qty);
    void fill_best_bid(Quantity executed_qty);

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