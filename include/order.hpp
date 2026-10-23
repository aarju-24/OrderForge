#ifndef ORDER_HPP
#define ORDER_HPP

#include "types.hpp"

struct Order {
    OrderId order_id;
    Side side;
    OrderType type;
    Price price;
    Quantity initial_quantity;
    Quantity remaining_quantity;
    OrderStatus status;

    bool is_active() const {
        return remaining_quantity > 0 && status != OrderStatus::Cancelled && status != OrderStatus::Rejected;
    }
};

#endif // ORDER_HPP