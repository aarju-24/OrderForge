#ifndef EVENT_READER_HPP
#define EVENT_READER_HPP

#include <string>
#include <vector>
#include "types.hpp"
#include "order.hpp"

enum class EventType {
    NewOrder,
    CancelOrder
};

struct Event {
    EventType type;
    Order order;
};

class EventReader {
public:
    static std::vector<Event> read_csv(const std::string& filepath);
};

#endif // EVENT_READER_HPP