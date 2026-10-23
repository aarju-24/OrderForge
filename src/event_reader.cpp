#include "event_reader.hpp"
#include <fstream>
#include <sstream>
#include <iostream>

std::vector<Event> EventReader::read_csv(const std::string& filepath) {
    std::vector<Event> events;
    std::ifstream file(filepath);

    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filepath << "\n";
        return events;
    }

    std::string line;
    std::getline(file, line); // Skip CSV header row

    while (std::getline(file, line)) {
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string type_str, id_str, side_str, price_str, qty_str;

        std::getline(ss, type_str, ',');
        std::getline(ss, id_str, ',');
        std::getline(ss, side_str, ',');
        std::getline(ss, price_str, ',');
        std::getline(ss, qty_str, ',');

        Event event;
        OrderId id = std::stoull(id_str);

        if (type_str == "NEW") {
            event.type = EventType::NewOrder;
            Side side = (side_str == "BUY") ? Side::Buy : Side::Sell;
            Price price = std::stoll(price_str);
            Quantity qty = std::stoul(qty_str);

            event.order = Order{id, side, OrderType::Limit, price, qty, qty, OrderStatus::New};
        } else if (type_str == "CANCEL") {
            event.type = EventType::CancelOrder;
            event.order.order_id = id;
        }

        events.push_back(event);
    }

    return events;
}