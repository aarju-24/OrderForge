// we need this to represent a trade execution match

#ifndef TRADE_HPP
#define TRADE_HPP

#include "types.hpp"

// Structure representing an executed trade execution match
struct Trade {
    OrderId buy_order_id;
    OrderId sell_order_id;
    Price execution_price;
    Quantity executed_quantity;
    Timestamp timestamp;
};

#endif // TRADE_HPP
