// we need this to represent a trade execution match

#ifndef TRADE_HPP
#define TRADE_HPP

#include "types.hpp"

// Structure representing an executed trade execution match
struct Trade {
    TradeId trade_id{0};
    OrderId buy_order_id{0};
    OrderId sell_order_id{0};
    Price execution_price{0};
    Quantity executed_quantity{0};
    Timestamp timestamp{0};
};

#endif // TRADE_HPP
