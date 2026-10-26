#include <gtest/gtest.h>
#include "event_reader.hpp"

// Test 1: Reading existing events.csv file returns expected events
TEST(EventReaderTest, ReadSampleCsv) {
    auto events = EventReader::read_csv("data/events.csv");
    ASSERT_EQ(events.size(), 5u);

    // Event 1: NEW,101,BUY,15000,100
    EXPECT_EQ(events[0].type, EventType::NewOrder);
    EXPECT_EQ(events[0].order.order_id, 101u);
    EXPECT_EQ(events[0].order.side, Side::Buy);
    EXPECT_EQ(events[0].order.price, 15000);
    EXPECT_EQ(events[0].order.initial_quantity, 100u);

    // Event 4: CANCEL,102,,,
    EXPECT_EQ(events[3].type, EventType::CancelOrder);
    EXPECT_EQ(events[3].order.order_id, 102u);
}

// Test 2: Reading a non-existent file returns empty vector safely
TEST(EventReaderTest, NonExistentFileReturnsEmpty) {
    auto events = EventReader::read_csv("data/does_not_exist.csv");
    EXPECT_TRUE(events.empty());
}
