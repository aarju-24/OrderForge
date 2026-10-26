#include <gtest/gtest.h>
#include "order_book.hpp"
#include "types.hpp"

// Test 1: Bid price ordering (highest price has priority)
TEST(OrderBookTest, BidPriceOrdering) {
    OrderBook book;
    Order o1{1, Side::Buy, OrderType::Limit, 100, 10, 10, OrderStatus::New};
    Order o2{2, Side::Buy, OrderType::Limit, 110, 10, 10, OrderStatus::New};
    Order o3{3, Side::Buy, OrderType::Limit, 105, 10, 10, OrderStatus::New};

    EXPECT_TRUE(book.add_order(o1));
    EXPECT_TRUE(book.add_order(o2));
    EXPECT_TRUE(book.add_order(o3));

    ASSERT_TRUE(book.best_bid().has_value());
    EXPECT_EQ(*book.best_bid(), 110);
    EXPECT_EQ(book.size(), 3u);
}

// Test 2: Ask price ordering (lowest price has priority)
TEST(OrderBookTest, AskPriceOrdering) {
    OrderBook book;
    Order o1{1, Side::Sell, OrderType::Limit, 200, 10, 10, OrderStatus::New};
    Order o2{2, Side::Sell, OrderType::Limit, 190, 10, 10, OrderStatus::New};
    Order o3{3, Side::Sell, OrderType::Limit, 195, 10, 10, OrderStatus::New};

    EXPECT_TRUE(book.add_order(o1));
    EXPECT_TRUE(book.add_order(o2));
    EXPECT_TRUE(book.add_order(o3));

    ASSERT_TRUE(book.best_ask().has_value());
    EXPECT_EQ(*book.best_ask(), 190);
    EXPECT_EQ(book.size(), 3u);
}

// Test 3: FIFO time priority at the same price level
TEST(OrderBookTest, FifoQueuePriority) {
    OrderBook book;
    Order o1{1, Side::Buy, OrderType::Limit, 100, 10, 10, OrderStatus::New};
    Order o2{2, Side::Buy, OrderType::Limit, 100, 20, 20, OrderStatus::New};

    EXPECT_TRUE(book.add_order(o1));
    EXPECT_TRUE(book.add_order(o2));

    // First in line should be o1
    EXPECT_EQ(book.best_bid_order().order_id, 1u);

    // After filling o1, o2 should be at the head of the queue
    book.fill_best_bid(10);
    EXPECT_EQ(book.best_bid_order().order_id, 2u);
    EXPECT_EQ(book.best_bid_order().remaining_quantity, 20u);
}

// Test 4: Cancellation of an active order and cleanup
TEST(OrderBookTest, CancelActiveOrder) {
    OrderBook book;
    Order o1{101, Side::Buy, OrderType::Limit, 15000, 100, 100, OrderStatus::New};
    EXPECT_TRUE(book.add_order(o1));

    EXPECT_EQ(book.size(), 1u);
    ASSERT_TRUE(book.best_bid().has_value());
    EXPECT_EQ(*book.best_bid(), 15000);

    auto cancelled_opt = book.cancel_order(101);
    ASSERT_TRUE(cancelled_opt.has_value());
    EXPECT_EQ(cancelled_opt->order_id, 101u);
    EXPECT_EQ(cancelled_opt->remaining_quantity, 100u);
    EXPECT_EQ(book.size(), 0u);
    EXPECT_FALSE(book.best_bid().has_value()); // Clean nullopt when empty
}

// Test 5: Cancellation of non-existent order
TEST(OrderBookTest, CancelNonExistentOrder) {
    OrderBook book;
    EXPECT_FALSE(book.cancel_order(999).has_value());
}

// Test 6: Empty book returns nullopt
TEST(OrderBookTest, EmptyBookReturnsNullopt) {
    OrderBook book;
    EXPECT_FALSE(book.best_bid().has_value());
    EXPECT_FALSE(book.best_ask().has_value());
    EXPECT_EQ(book.size(), 0u);
}

// Test 7: Duplicate Order ID is rejected and does not corrupt existing orders
TEST(OrderBookTest, DuplicateOrderIdIsRejected) {
    OrderBook book;
    Order o1{101, Side::Buy, OrderType::Limit, 15000, 100, 100, OrderStatus::New};
    Order o2{101, Side::Buy, OrderType::Limit, 14900, 50, 50, OrderStatus::New};

    EXPECT_TRUE(book.add_order(o1));
    EXPECT_FALSE(book.add_order(o2)); // Must reject duplicate active ID!

    EXPECT_EQ(book.size(), 1u);
    ASSERT_TRUE(book.best_bid().has_value());
    EXPECT_EQ(*book.best_bid(), 15000);

    // Verify o1 can still be safely cancelled
    EXPECT_TRUE(book.cancel_order(101).has_value());
    EXPECT_EQ(book.size(), 0u);
    EXPECT_FALSE(book.best_bid().has_value());
}

// Test 8: Depth aggregation across multiple price levels and orders
TEST(OrderBookTest, DepthAggregation) {
    OrderBook book;

    // 2 orders at 15000 (qty 100, 50), 1 at 14950 (qty 200)
    book.add_order(Order{1, Side::Buy, OrderType::Limit, 15000, 100, 100, OrderStatus::New});
    book.add_order(Order{2, Side::Buy, OrderType::Limit, 15000, 50, 50, OrderStatus::New});
    book.add_order(Order{3, Side::Buy, OrderType::Limit, 14950, 200, 200, OrderStatus::New});

    // 1 order at 15100 (qty 80), 1 at 15150 (qty 120)
    book.add_order(Order{4, Side::Sell, OrderType::Limit, 15100, 80, 80, OrderStatus::New});
    book.add_order(Order{5, Side::Sell, OrderType::Limit, 15150, 120, 120, OrderStatus::New});

    EXPECT_EQ(book.total_bid_volume(), 350u);
    EXPECT_EQ(book.total_ask_volume(), 200u);
    EXPECT_EQ(book.volume_at_price(Side::Buy, 15000), 150u);
    EXPECT_EQ(book.volume_at_price(Side::Buy, 14950), 200u);
    EXPECT_EQ(book.volume_at_price(Side::Sell, 15100), 80u);

    auto bid_depth = book.get_bid_depth();
    ASSERT_EQ(bid_depth.size(), 2u);
    EXPECT_EQ(bid_depth[0].price, 15000);
    EXPECT_EQ(bid_depth[0].total_quantity, 150u);
    EXPECT_EQ(bid_depth[0].order_count, 2u);

    EXPECT_EQ(bid_depth[1].price, 14950);
    EXPECT_EQ(bid_depth[1].total_quantity, 200u);
    EXPECT_EQ(bid_depth[1].order_count, 1u);

    EXPECT_TRUE(book.check_invariants());
}


