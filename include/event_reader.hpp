#ifndef EVENT_READER_HPP
#define EVENT_READER_HPP

#include <string>
#include <vector>
#include <iostream>
#include "types.hpp"
#include "order.hpp"

enum class EventType {
    NewOrder,
    CancelOrder
};

inline std::ostream& operator<<(std::ostream& os, EventType type) {
    return os << (type == EventType::NewOrder ? "NEW" : "CANCEL");
}

struct Event {
    uint64_t sequence_number{0};
    EventType type{EventType::NewOrder};
    Order order{};
    size_t line_number{0};
};

struct ParseError {
    size_t line_number{0};
    std::string raw_line;
    std::string error_message;
};

struct ParseResult {
    std::vector<Event> events;
    std::vector<ParseError> errors;
    size_t total_lines_read{0};
};

class EventReader {
public:
    static ParseResult parse_csv(const std::string& filepath);
    static std::vector<Event> read_csv(const std::string& filepath);

    // Safe string and numeric utility parsers
    static std::string trim(const std::string& str);
    static std::string to_upper(std::string str);
    static bool parse_uint64(const std::string& str, uint64_t& out);
    static bool parse_int64(const std::string& str, int64_t& out);
    static bool parse_uint32(const std::string& str, uint32_t& out);
    static bool parse_side(const std::string& str, Side& out);
    static bool parse_order_type(const std::string& str, OrderType& out);
};

#endif // EVENT_READER_HPP