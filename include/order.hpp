#ifndef ORDER_HPP
#define ORDER_HPP

#include "types.hpp"

struct Order {
    OrderId order_id{0};
    Side side{Side::Buy};
    OrderType type{OrderType::Limit};
    Price price{0};
    Quantity initial_quantity{0};
    Quantity remaining_quantity{0};
    OrderStatus status{OrderStatus::New};

    Quantity executed_quantity() const {
        return initial_quantity >= remaining_quantity ? initial_quantity - remaining_quantity : 0;
    }

    bool is_active() const {
        return remaining_quantity > 0 && status != OrderStatus::Cancelled && status != OrderStatus::Rejected;
    }
};

#endif // ORDER_HPP