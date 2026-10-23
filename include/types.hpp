#ifndef TYPES_HPP
#define TYPES_HPP

#include <cstdint>
#include <iostream>

// Fixed-width integer aliases
using OrderId   = uint64_t;
using Price     = int64_t;   // Ticks
using Quantity  = uint32_t;
using Timestamp = uint64_t;

// Enums for type safety
enum class Side : uint8_t {
    Buy,
    Sell
};

enum class OrderType : uint8_t {
    Limit,
    Market
};

enum class OrderStatus : uint8_t {
    New,
    PartiallyFilled,
    Filled,
    Cancelled,
    Rejected
};

// Stream operator for printing Side
inline std::ostream& operator<<(std::ostream& os, Side side) {
    return os << (side == Side::Buy ? "BUY" : "SELL");
}

#endif // TYPES_HPP