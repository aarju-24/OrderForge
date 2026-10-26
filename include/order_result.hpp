#ifndef ORDER_RESULT_HPP
#define ORDER_RESULT_HPP

#include <vector>
#include <string>
#include "types.hpp"
#include "trade.hpp"

// Structured outcome report for order operations
struct OrderResult {
    OrderId order_id{0};
    OrderStatus status{OrderStatus::Rejected};
    OrderType order_type{OrderType::Limit};
    Side side{Side::Buy};
    Price price{0};
    Quantity initial_quantity{0};
    Quantity executed_quantity{0};
    Quantity remaining_quantity{0};
    Quantity cancelled_quantity{0};
    std::vector<Trade> trades{};
    std::string reject_reason{};

    bool is_accepted() const {
        return status != OrderStatus::Rejected;
    }

    // Mathematical verification of quantity conservation
    bool verify_accounting() const {
        if (status == OrderStatus::Rejected) {
            return executed_quantity == 0 && remaining_quantity == 0 && cancelled_quantity == 0;
        }
        return initial_quantity == (executed_quantity + remaining_quantity + cancelled_quantity);
    }
};

#endif // ORDER_RESULT_HPP
