#include <gtest/gtest.h>
#include "matching_engine.hpp"

// Test 1: Non-crossing limit orders rest in the book without generating trades
TEST(MatchingEngineTest, NonCrossingLimitOrders) {
    MatchingEngine engine;

    Order buy{1, Side::Buy, OrderType::Limit, 15000, 100, 100, OrderStatus::New};
    auto res1 = engine.process_order(buy);
    EXPECT_TRUE(res1.is_accepted());
    EXPECT_EQ(res1.status, OrderStatus::New);
    EXPECT_EQ(res1.executed_quantity, 0u);
    EXPECT_EQ(res1.remaining_quantity, 100u);
    EXPECT_EQ(res1.cancelled_quantity, 0u);
    EXPECT_TRUE(res1.verify_accounting());
    EXPECT_TRUE(res1.trades.empty());
    EXPECT_EQ(engine.book().size(), 1u);
    ASSERT_TRUE(engine.book().best_bid().has_value());
    EXPECT_EQ(*engine.book().best_bid(), 15000);

    Order sell{2, Side::Sell, OrderType::Limit, 15100, 100, 100, OrderStatus::New};
    auto res2 = engine.process_order(sell);
    EXPECT_TRUE(res2.is_accepted());
    EXPECT_EQ(res2.status, OrderStatus::New);
    EXPECT_TRUE(res2.verify_accounting());
    EXPECT_TRUE(res2.trades.empty());
    EXPECT_EQ(engine.book().size(), 2u);
    ASSERT_TRUE(engine.book().best_ask().has_value());
    EXPECT_EQ(*engine.book().best_ask(), 15100);
}

// Test 2: Exact fill matches at the resting order's price
TEST(MatchingEngineTest, ExactFillAtRestingPrice) {
    MatchingEngine engine;

    // Resting SELL at 15000
    Order sell{1, Side::Sell, OrderType::Limit, 15000, 100, 100, OrderStatus::New};
    engine.process_order(sell);

    // Incoming aggressive BUY at 15050 (willing to pay up to 15050)
    Order buy{2, Side::Buy, OrderType::Limit, 15050, 100, 100, OrderStatus::New};
    auto res = engine.process_order(buy);

    EXPECT_TRUE(res.is_accepted());
    EXPECT_EQ(res.status, OrderStatus::Filled);
    EXPECT_EQ(res.executed_quantity, 100u);
    EXPECT_EQ(res.remaining_quantity, 0u);
    EXPECT_TRUE(res.verify_accounting());

    ASSERT_EQ(res.trades.size(), 1u);
    EXPECT_EQ(res.trades[0].buy_order_id, 2u);
    EXPECT_EQ(res.trades[0].sell_order_id, 1u);
    EXPECT_EQ(res.trades[0].execution_price, 15000); // Must execute at resting ask price!
    EXPECT_EQ(res.trades[0].executed_quantity, 100u);

    // Both orders fully filled -> book is now empty
    EXPECT_EQ(engine.book().size(), 0u);
}

// Test 3: Partial fill of incoming order leaves remainder resting on the book
TEST(MatchingEngineTest, PartialFillIncomingOrderRestsRemainder) {
    MatchingEngine engine;

    // Resting SELL: 50 @ 15000
    Order sell{1, Side::Sell, OrderType::Limit, 15000, 50, 50, OrderStatus::New};
    engine.process_order(sell);

    // Incoming BUY: 100 @ 15000
    Order buy{2, Side::Buy, OrderType::Limit, 15000, 100, 100, OrderStatus::New};
    auto res = engine.process_order(buy);

    EXPECT_TRUE(res.is_accepted());
    EXPECT_EQ(res.status, OrderStatus::PartiallyFilled);
    EXPECT_EQ(res.executed_quantity, 50u);
    EXPECT_EQ(res.remaining_quantity, 50u);
    EXPECT_EQ(res.cancelled_quantity, 0u);
    EXPECT_TRUE(res.verify_accounting());

    ASSERT_EQ(res.trades.size(), 1u);
    EXPECT_EQ(res.trades[0].executed_quantity, 50u);

    // Remaining 50 should now rest on the buy side
    EXPECT_EQ(engine.book().size(), 1u);
    ASSERT_TRUE(engine.book().best_bid().has_value());
    EXPECT_EQ(*engine.book().best_bid(), 15000);
    EXPECT_FALSE(engine.book().best_ask().has_value());
}

// Test 4: Partial fill of resting order preserves its queue position and reduces quantity
TEST(MatchingEngineTest, PartialFillRestingOrderMaintainsQueue) {
    MatchingEngine engine;

    // Resting SELL: 100 @ 15000
    Order sell{1, Side::Sell, OrderType::Limit, 15000, 100, 100, OrderStatus::New};
    engine.process_order(sell);

    // Incoming BUY: 40 @ 15000
    Order buy{2, Side::Buy, OrderType::Limit, 15000, 40, 40, OrderStatus::New};
    auto res = engine.process_order(buy);

    EXPECT_EQ(res.status, OrderStatus::Filled);
    EXPECT_EQ(res.executed_quantity, 40u);
    EXPECT_TRUE(res.verify_accounting());

    ASSERT_EQ(res.trades.size(), 1u);
    EXPECT_EQ(res.trades[0].executed_quantity, 40u);

    // Resting sell should still be in the book with 60 remaining
    EXPECT_EQ(engine.book().size(), 1u);
    ASSERT_TRUE(engine.book().best_ask().has_value());
    EXPECT_EQ(*engine.book().best_ask(), 15000);
    EXPECT_EQ(engine.book().best_ask_order().remaining_quantity, 60u);
}

// Test 5: Multi-level sweep across multiple price levels
TEST(MatchingEngineTest, MultiLevelSweep) {
    MatchingEngine engine;

    engine.process_order(Order{1, Side::Sell, OrderType::Limit, 15000, 50, 50, OrderStatus::New});
    engine.process_order(Order{2, Side::Sell, OrderType::Limit, 15050, 50, 50, OrderStatus::New});

    // Incoming BUY sweeps 80 @ 15050
    Order buy{3, Side::Buy, OrderType::Limit, 15050, 80, 80, OrderStatus::New};
    auto res = engine.process_order(buy);

    EXPECT_EQ(res.status, OrderStatus::Filled);
    EXPECT_EQ(res.executed_quantity, 80u);
    EXPECT_TRUE(res.verify_accounting());

    ASSERT_EQ(res.trades.size(), 2u);
    EXPECT_EQ(res.trades[0].execution_price, 15000);
    EXPECT_EQ(res.trades[0].executed_quantity, 50u);

    EXPECT_EQ(res.trades[1].execution_price, 15050);
    EXPECT_EQ(res.trades[1].executed_quantity, 30u);

    // Level 1 cleared, Level 2 has 20 remaining
    EXPECT_EQ(engine.book().size(), 1u);
    ASSERT_TRUE(engine.book().best_ask().has_value());
    EXPECT_EQ(*engine.book().best_ask(), 15050);
}

// Test 6: Market order sweeps available depth and cancels unfilled remainder
TEST(MatchingEngineTest, MarketOrderSweepsAndCancelsRemainder) {
    MatchingEngine engine;

    engine.process_order(Order{1, Side::Sell, OrderType::Limit, 15000, 50, 50, OrderStatus::New});
    engine.process_order(Order{2, Side::Sell, OrderType::Limit, 15050, 50, 50, OrderStatus::New});

    // Incoming Market BUY for 120 (only 100 available in book)
    Order mkt_buy{3, Side::Buy, OrderType::Market, 0, 120, 120, OrderStatus::New};
    auto res = engine.process_order(mkt_buy);

    EXPECT_TRUE(res.is_accepted());
    EXPECT_EQ(res.status, OrderStatus::PartiallyFilled);
    EXPECT_EQ(res.executed_quantity, 100u);
    EXPECT_EQ(res.remaining_quantity, 0u);
    EXPECT_EQ(res.cancelled_quantity, 20u); // Unfilled remainder cancelled!
    EXPECT_TRUE(res.verify_accounting());   // 120 == 100 + 0 + 20

    ASSERT_EQ(res.trades.size(), 2u);
    EXPECT_EQ(res.trades[0].executed_quantity, 50u);
    EXPECT_EQ(res.trades[1].executed_quantity, 50u);

    // Book is completely empty
    EXPECT_EQ(engine.book().size(), 0u);
}

// Test 7: Market order with no liquidity is cancelled completely
TEST(MatchingEngineTest, MarketOrderWithNoLiquidityCancelled) {
    MatchingEngine engine;

    Order mkt_buy{1, Side::Buy, OrderType::Market, 0, 100, 100, OrderStatus::New};
    auto res = engine.process_order(mkt_buy);

    EXPECT_TRUE(res.is_accepted());
    EXPECT_EQ(res.status, OrderStatus::Cancelled);
    EXPECT_EQ(res.executed_quantity, 0u);
    EXPECT_EQ(res.remaining_quantity, 0u);
    EXPECT_EQ(res.cancelled_quantity, 100u);
    EXPECT_TRUE(res.verify_accounting()); // 100 == 0 + 0 + 100
    EXPECT_TRUE(res.trades.empty());
    EXPECT_EQ(engine.book().size(), 0u);
}

// Test 8: Order with 0 quantity is rejected
TEST(MatchingEngineTest, ZeroQuantityRejected) {
    MatchingEngine engine;

    Order zero_qty{1, Side::Buy, OrderType::Limit, 15000, 0, 0, OrderStatus::New};
    auto res = engine.process_order(zero_qty);

    EXPECT_FALSE(res.is_accepted());
    EXPECT_EQ(res.status, OrderStatus::Rejected);
    EXPECT_EQ(res.reject_reason, "Order quantity must be positive");
    EXPECT_TRUE(res.verify_accounting());
    EXPECT_EQ(engine.book().size(), 0u);
}

// Test 9: Limit order with negative or zero price is rejected
TEST(MatchingEngineTest, LimitOrderPriceValidation) {
    MatchingEngine engine;

    Order non_pos_price{1, Side::Buy, OrderType::Limit, 0, 100, 100, OrderStatus::New};
    auto res = engine.process_order(non_pos_price);

    EXPECT_FALSE(res.is_accepted());
    EXPECT_EQ(res.status, OrderStatus::Rejected);
    EXPECT_EQ(res.reject_reason, "Limit price must be positive");
}

// Test 10: Tick size alignment validation
TEST(MatchingEngineTest, TickSizeValidation) {
    MatchingEngine engine(10); // Tick size = 10

    Order valid_tick{1, Side::Buy, OrderType::Limit, 15010, 100, 100, OrderStatus::New};
    auto res1 = engine.process_order(valid_tick);
    EXPECT_TRUE(res1.is_accepted());

    Order invalid_tick{2, Side::Buy, OrderType::Limit, 15015, 100, 100, OrderStatus::New};
    auto res2 = engine.process_order(invalid_tick);
    EXPECT_FALSE(res2.is_accepted());
    EXPECT_EQ(res2.status, OrderStatus::Rejected);
    EXPECT_EQ(res2.reject_reason, "Price violates configured tick size");
}

// Test 11: Cancellation of active order maintains accounting identity
TEST(MatchingEngineTest, CancelActiveOrderAccounting) {
    MatchingEngine engine;

    Order buy{1, Side::Buy, OrderType::Limit, 15000, 100, 100, OrderStatus::New};
    engine.process_order(buy);

    auto cancel_res = engine.cancel_order(1);
    EXPECT_TRUE(cancel_res.is_accepted());
    EXPECT_EQ(cancel_res.status, OrderStatus::Cancelled);
    EXPECT_EQ(cancel_res.initial_quantity, 100u);
    EXPECT_EQ(cancel_res.executed_quantity, 0u);
    EXPECT_EQ(cancel_res.cancelled_quantity, 100u);
    EXPECT_EQ(cancel_res.remaining_quantity, 0u);
    EXPECT_TRUE(cancel_res.verify_accounting()); // 100 == 0 + 0 + 100

    EXPECT_EQ(engine.book().size(), 0u);
}

// Test 12: Cancellation of partially filled order maintains accounting identity
TEST(MatchingEngineTest, CancelPartiallyFilledOrderAccounting) {
    MatchingEngine engine;

    // Resting SELL: 100 @ 15000
    engine.process_order(Order{1, Side::Sell, OrderType::Limit, 15000, 100, 100, OrderStatus::New});

    // Match 30
    engine.process_order(Order{2, Side::Buy, OrderType::Limit, 15000, 30, 30, OrderStatus::New});

    // Now cancel Order 1 (initial: 100, executed: 30, remaining in book: 70)
    auto cancel_res = engine.cancel_order(1);
    EXPECT_TRUE(cancel_res.is_accepted());
    EXPECT_EQ(cancel_res.status, OrderStatus::Cancelled);
    EXPECT_EQ(cancel_res.initial_quantity, 100u);
    EXPECT_EQ(cancel_res.executed_quantity, 30u);
    EXPECT_EQ(cancel_res.cancelled_quantity, 70u);
    EXPECT_EQ(cancel_res.remaining_quantity, 0u);
    EXPECT_TRUE(cancel_res.verify_accounting()); // 100 == 30 + 0 + 70!

    EXPECT_EQ(engine.book().size(), 0u);
}

// Test 13: Cancellation of already cancelled or filled order is rejected
TEST(MatchingEngineTest, CancelTerminalOrderIsRejected) {
    MatchingEngine engine;

    engine.process_order(Order{1, Side::Buy, OrderType::Limit, 15000, 100, 100, OrderStatus::New});
    auto cancel_res = engine.cancel_order(1);
    EXPECT_TRUE(cancel_res.is_accepted());

    // Try cancelling again
    auto cancel_again = engine.cancel_order(1);
    EXPECT_FALSE(cancel_again.is_accepted());
    EXPECT_EQ(cancel_again.status, OrderStatus::Rejected);
    EXPECT_EQ(cancel_again.reject_reason, "Order is already filled or cancelled");
}

// Test 14: Duplicate incoming active or historical order ID is rejected
TEST(MatchingEngineTest, DuplicateActiveOrderIdIsRejected) {
    MatchingEngine engine;

    Order buy1{101, Side::Buy, OrderType::Limit, 15000, 100, 100, OrderStatus::New};
    auto res1 = engine.process_order(buy1);
    EXPECT_TRUE(res1.is_accepted());

    // Incoming duplicate ID 101
    Order buy2{101, Side::Buy, OrderType::Limit, 14900, 50, 50, OrderStatus::New};
    auto res2 = engine.process_order(buy2);
    EXPECT_FALSE(res2.is_accepted());
    EXPECT_EQ(res2.status, OrderStatus::Rejected);
    EXPECT_EQ(res2.reject_reason, "Duplicate order ID");

    // Book state remains untouched
    EXPECT_EQ(engine.book().size(), 1u);
    ASSERT_TRUE(engine.book().best_bid().has_value());
    EXPECT_EQ(*engine.book().best_bid(), 15000);
}
