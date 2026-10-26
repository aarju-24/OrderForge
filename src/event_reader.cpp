#include "event_reader.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

std::string EventReader::trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

std::string EventReader::to_upper(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return str;
}

bool EventReader::parse_uint64(const std::string& str, uint64_t& out) {
    std::string s = trim(str);
    if (s.empty()) return false;
    try {
        size_t idx = 0;
        unsigned long long val = std::stoull(s, &idx);
        if (idx != s.size()) return false;
        out = static_cast<uint64_t>(val);
        return true;
    } catch (...) {
        return false;
    }
}

bool EventReader::parse_int64(const std::string& str, int64_t& out) {
    std::string s = trim(str);
    if (s.empty()) return false;
    try {
        size_t idx = 0;
        long long val = std::stoll(s, &idx);
        if (idx != s.size()) return false;
        out = static_cast<int64_t>(val);
        return true;
    } catch (...) {
        return false;
    }
}

bool EventReader::parse_uint32(const std::string& str, uint32_t& out) {
    std::string s = trim(str);
    if (s.empty()) return false;
    try {
        size_t idx = 0;
        unsigned long val = std::stoul(s, &idx);
        if (idx != s.size()) return false;
        out = static_cast<uint32_t>(val);
        return true;
    } catch (...) {
        return false;
    }
}

bool EventReader::parse_side(const std::string& str, Side& out) {
    std::string s = to_upper(trim(str));
    if (s == "BUY") {
        out = Side::Buy;
        return true;
    }
    if (s == "SELL") {
        out = Side::Sell;
        return true;
    }
    return false;
}

bool EventReader::parse_order_type(const std::string& str, OrderType& out) {
    std::string s = to_upper(trim(str));
    if (s.empty() || s == "LIMIT") {
        out = OrderType::Limit;
        return true;
    }
    if (s == "MARKET") {
        out = OrderType::Market;
        return true;
    }
    return false;
}

ParseResult EventReader::parse_csv(const std::string& filepath) {
    ParseResult result;
    std::ifstream file(filepath);

    if (!file.is_open()) {
        result.errors.push_back(ParseError{0, "", "Could not open file: " + filepath});
        return result;
    }

    std::string line;
    size_t line_num = 0;
    uint64_t seq = 0;
    bool is_first_line = true;

    while (std::getline(file, line)) {
        ++line_num;
        ++result.total_lines_read;

        std::string trimmed_line = trim(line);
        if (trimmed_line.empty()) continue;

        // Header check on first non-empty line
        if (is_first_line) {
            is_first_line = false;
            std::string upper_line = to_upper(trimmed_line);
            if (upper_line.find("TYPE") != std::string::npos || upper_line.find("ORDERID") != std::string::npos) {
                continue; // Skip CSV header
            }
        }

        std::stringstream ss(trimmed_line);
        std::vector<std::string> tokens;
        std::string token;
        while (std::getline(ss, token, ',')) {
            tokens.push_back(trim(token));
        }

        if (tokens.empty()) continue;

        std::string type_str = to_upper(tokens[0]);

        if (type_str == "NEW") {
            if (tokens.size() < 5) {
                result.errors.push_back(ParseError{line_num, line, "NEW order requires at least 5 columns (Type,OrderId,Side,Price,Quantity)"});
                continue;
            }

            OrderId id = 0;
            if (!parse_uint64(tokens[1], id)) {
                result.errors.push_back(ParseError{line_num, line, "Invalid OrderId: '" + tokens[1] + "'"});
                continue;
            }

            Side side = Side::Buy;
            if (!parse_side(tokens[2], side)) {
                result.errors.push_back(ParseError{line_num, line, "Invalid Side: '" + tokens[2] + "' (expected BUY or SELL)"});
                continue;
            }

            Price price = 0;
            if (!parse_int64(tokens[3], price)) {
                result.errors.push_back(ParseError{line_num, line, "Invalid Price: '" + tokens[3] + "'"});
                continue;
            }

            Quantity qty = 0;
            if (!parse_uint32(tokens[4], qty)) {
                result.errors.push_back(ParseError{line_num, line, "Invalid Quantity: '" + tokens[4] + "'"});
                continue;
            }

            OrderType order_type = OrderType::Limit;
            if (tokens.size() >= 6) {
                if (!parse_order_type(tokens[5], order_type)) {
                    result.errors.push_back(ParseError{line_num, line, "Invalid OrderType: '" + tokens[5] + "' (expected LIMIT or MARKET)"});
                    continue;
                }
            }

            Event event;
            event.sequence_number = ++seq;
            event.line_number = line_num;
            event.type = EventType::NewOrder;
            event.order = Order{id, side, order_type, price, qty, qty, OrderStatus::New};
            result.events.push_back(event);

        } else if (type_str == "CANCEL") {
            if (tokens.size() < 2) {
                result.errors.push_back(ParseError{line_num, line, "CANCEL requires at least 2 columns (Type,OrderId)"});
                continue;
            }

            OrderId id = 0;
            if (!parse_uint64(tokens[1], id)) {
                result.errors.push_back(ParseError{line_num, line, "Invalid OrderId for CANCEL: '" + tokens[1] + "'"});
                continue;
            }

            Event event;
            event.sequence_number = ++seq;
            event.line_number = line_num;
            event.type = EventType::CancelOrder;
            event.order.order_id = id;
            result.events.push_back(event);

        } else {
            result.errors.push_back(ParseError{line_num, line, "Unknown Event Type: '" + tokens[0] + "' (expected NEW or CANCEL)"});
        }
    }

    return result;
}

std::vector<Event> EventReader::read_csv(const std::string& filepath) {
    ParseResult result = parse_csv(filepath);
    for (const auto& err : result.errors) {
        if (err.line_number > 0) {
            std::cerr << "[CSV Warning] Line " << err.line_number << ": " << err.error_message << " (Raw: " << err.raw_line << ")\n";
        } else {
            std::cerr << "[CSV Error] " << err.error_message << "\n";
        }
    }
    return result.events;
}