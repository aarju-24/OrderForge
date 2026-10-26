#ifndef TYPES_HPP
#define TYPES_HPP

#include <cstdint>
#include <iostream>
#include <stdexcept>

// Fixed-width integer aliases
using OrderId   = uint64_t;
using TradeId   = uint64_t;
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

// Stream operators
inline std::ostream& operator<<(std::ostream& os, Side side) {
    return os << (side == Side::Buy ? "BUY" : "SELL");
}

inline std::ostream& operator<<(std::ostream& os, OrderType type) {
    return os << (type == OrderType::Limit ? "LIMIT" : "MARKET");
}

inline std::ostream& operator<<(std::ostream& os, OrderStatus status) {
    switch (status) {
        case OrderStatus::New: return os << "NEW";
        case OrderStatus::PartiallyFilled: return os << "PARTIALLY_FILLED";
        case OrderStatus::Filled: return os << "FILLED";
        case OrderStatus::Cancelled: return os << "CANCELLED";
        case OrderStatus::Rejected: return os << "REJECTED";
    }
    return os << "UNKNOWN";
}


// -------------------------------------------------------------------------
// Optional type compatibility
// Uses std::optional on C++17 compilers, and a zero-overhead polyfill on GCC 6.3
// -------------------------------------------------------------------------
#if defined(__has_include) && __has_include(<optional>) && __cplusplus >= 201703L
#  include <optional>
   template <typename T>
   using Optional = std::optional<T>;
   constexpr auto NullOpt = std::nullopt;
#else
struct NullOptType {
    explicit constexpr NullOptType(int) {}
};
constexpr NullOptType NullOpt{0};

template <typename T>
class Optional {
public:
    constexpr Optional() noexcept : has_val_(false), val_{} {}
    constexpr Optional(NullOptType) noexcept : has_val_(false), val_{} {}
    constexpr Optional(const T& val) noexcept : has_val_(true), val_(val) {}

    constexpr bool has_value() const noexcept { return has_val_; }
    constexpr explicit operator bool() const noexcept { return has_val_; }

    constexpr const T& operator*() const noexcept { return val_; }
    constexpr const T* operator->() const noexcept { return &val_; }
    constexpr const T& value() const {
        if (!has_val_) throw std::runtime_error("Bad Optional access");
        return val_;
    }
    constexpr T value_or(const T& default_val) const noexcept {
        return has_val_ ? val_ : default_val;
    }

    constexpr bool operator==(const Optional& other) const noexcept {
        if (has_val_ != other.has_val_) return false;
        return !has_val_ || (val_ == other.val_);
    }
    constexpr bool operator!=(const Optional& other) const noexcept {
        return !(*this == other);
    }
    constexpr bool operator==(const T& other_val) const noexcept {
        return has_val_ && (val_ == other_val);
    }
    constexpr bool operator!=(const T& other_val) const noexcept {
        return !(*this == other_val);
    }

private:
    bool has_val_;
    T val_;
};
#endif

template <typename T>
inline std::ostream& operator<<(std::ostream& os, const Optional<T>& opt) {
    if (opt.has_value()) {
        return os << *opt;
    }
    return os << "None";
}

#endif // TYPES_HPP