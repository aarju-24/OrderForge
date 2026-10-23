#ifndef MATCHING_ENGINE_HPP
#define MATCHING_ENGINE_HPP

#include <vector>
#include "order_book.hpp"
#include "trade.hpp"
#include "types.hpp"
#include "order.hpp"

class MatchingEngine {
public:
    // process an incoming order and return all generated execution trades 
    std::vector<Trade> process_order(Order incoming_order);
    
    // cancel an order by passing request to the underlyingbook 
    bool cancel_order(OrderId order_id){
        return book_.cancel_order(order_id);
    }
    // access underlying OrderBook state 
    const OrderBook& book() const { return book_;}
private:
    OrderBook book_;
    Timestamp current_timestamp_{10000};
};
#endif // MATCHING_ENGINE_HPP