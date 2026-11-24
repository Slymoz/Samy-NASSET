#include "Backtester.hpp"
#include "TestHelpers.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

using test::candle;
using test::flatCandles;
using test::frictionless;
using test::ScriptedStrategy;

TEST(Backtester, SignalIsFilledAtNextOpen) {
    // The BUY decided at the close of bar 0 (100) is filled at the open of bar 1 (110).
    const std::vector<Candle> candles = {candle(0, 100, 100, 100, 100), candle(1, 110, 120, 110, 120),
                                         candle(2, 130, 130, 130, 130), candle(3, 140, 140, 140, 140)};
    ScriptedStrategy strategy({{0, Signal::BUY}, {1, Signal::SELL}});
    const auto result = Backtester(frictionless()).run(candles, strategy);

    ASSERT_EQ(result.trades.size(), 1U);
    const auto &trade = result.trades[0];
    EXPECT_EQ(trade.entryTimestamp, candles[1].timestamp);
    EXPECT_DOUBLE_EQ(trade.entryPrice, 110.0);
    EXPECT_EQ(trade.exitTimestamp, candles[2].timestamp);
    EXPECT_DOUBLE_EQ(trade.exitPrice, 130.0);
    EXPECT_EQ(trade.exitReason, ExitReason::SIGNAL);
    EXPECT_NEAR(result.finalCapital, 1000.0 * 130.0 / 110.0, 1e-9);
}

TEST(Backtester, FeesAndSlippageAreChargedOnBothSides) {
    BacktestConfig config;
    config.initialCapital = 1000.0;
    config.feeRate = 0.01;
    config.slippageRate = 0.02;
    const auto candles = flatCandles({100, 100, 100, 100});
    ScriptedStrategy strategy({{0, Signal::BUY}, {1, Signal::SELL}});
    const auto result = Backtester(config).run(candles, strategy);

    ASSERT_EQ(result.trades.size(), 1U);
    const auto &trade = result.trades[0];
    EXPECT_DOUBLE_EQ(trade.entryPrice, 102.0);
    EXPECT_DOUBLE_EQ(trade.exitPrice, 98.0);
    const double quantity = 1000.0 / (102.0 * 1.01);
    EXPECT_NEAR(trade.quantity, quantity, 1e-12);
    EXPECT_NEAR(trade.entryFee, quantity * 102.0 * 0.01, 1e-9);
    const double expected = quantity * 98.0 * 0.99;
    EXPECT_NEAR(result.finalCapital, expected, 1e-9);
    EXPECT_NEAR(trade.pnl, expected - 1000.0, 1e-9);
    EXPECT_LT(trade.pnl, 0.0);
}

TEST(Backtester, StopLossTriggersOnTheLowAtTheStopPrice) {
    BacktestConfig config = frictionless();
    config.stopLossPct = 10.0;
    const std::vector<Candle> candles = {candle(0, 100, 100, 100, 100), candle(1, 100, 101, 99, 100),
                                         candle(2, 98, 99, 85, 97), candle(3, 97, 97, 97, 97)};
    ScriptedStrategy strategy({{0, Signal::BUY}});
    const auto result = Backtester(config).run(candles, strategy);

    ASSERT_EQ(result.trades.size(), 1U);
    EXPECT_EQ(result.trades[0].exitReason, ExitReason::STOP_LOSS);
    EXPECT_DOUBLE_EQ(result.trades[0].exitPrice, 90.0); // not the close (97)
    EXPECT_EQ(result.trades[0].exitTimestamp, candles[2].timestamp);
    EXPECT_NEAR(result.roi, -10.0, 1e-9);
}

TEST(Backtester, GapThroughTheStopFillsAtTheOpen) {
    BacktestConfig config = frictionless();
    config.stopLossPct = 10.0;
    const std::vector<Candle> candles = {candle(0, 100, 100, 100, 100), candle(1, 100, 100, 100, 100),
                                         candle(2, 80, 85, 75, 82)};
    ScriptedStrategy strategy({{0, Signal::BUY}});
    const auto result = Backtester(config).run(candles, strategy);

    ASSERT_EQ(result.trades.size(), 1U);
    EXPECT_DOUBLE_EQ(result.trades[0].exitPrice, 80.0);
}

TEST(Backtester, TakeProfitTriggersOnTheHigh) {
    BacktestConfig config = frictionless();
    config.takeProfitPct = 20.0;
    const std::vector<Candle> candles = {candle(0, 100, 100, 100, 100), candle(1, 100, 100, 100, 100),
                                         candle(2, 105, 125, 104, 110), candle(3, 110, 110, 110, 110)};
    ScriptedStrategy strategy({{0, Signal::BUY}});
    const auto result = Backtester(config).run(candles, strategy);

    ASSERT_EQ(result.trades.size(), 1U);
    EXPECT_EQ(result.trades[0].exitReason, ExitReason::TAKE_PROFIT);
    EXPECT_DOUBLE_EQ(result.trades[0].exitPrice, 120.0);
}

TEST(Backtester, StopLossWinsWhenBothLevelsAreHitInTheSameBar) {
    BacktestConfig config = frictionless();
    config.stopLossPct = 5.0;
    config.takeProfitPct = 5.0;
    const std::vector<Candle> candles = {candle(0, 100, 100, 100, 100), candle(1, 100, 100, 100, 100),
                                         candle(2, 100, 110, 90, 100)};
    ScriptedStrategy strategy({{0, Signal::BUY}});
    const auto result = Backtester(config).run(candles, strategy);

    ASSERT_EQ(result.trades.size(), 1U);
    EXPECT_EQ(result.trades[0].exitReason, ExitReason::STOP_LOSS);
}

TEST(Backtester, OpenPositionIsClosedAtTheLastClose) {
    const auto candles = flatCandles({100, 100, 150});
    ScriptedStrategy strategy({{0, Signal::BUY}});
    const auto result = Backtester(frictionless()).run(candles, strategy);

    ASSERT_EQ(result.trades.size(), 1U);
    EXPECT_EQ(result.trades[0].exitReason, ExitReason::END_OF_DATA);
    EXPECT_DOUBLE_EQ(result.finalCapital, 1500.0);
    EXPECT_DOUBLE_EQ(result.equityCurve.back().equity, 1500.0);
    EXPECT_DOUBLE_EQ(result.roi, 50.0);
}

TEST(Backtester, SignalOnTheLastBarIsIgnored) {
    const auto candles = flatCandles({100, 100, 100});
    ScriptedStrategy strategy({{2, Signal::BUY}});
    const auto result = Backtester(frictionless()).run(candles, strategy);
    EXPECT_TRUE(result.trades.empty());
    EXPECT_DOUBLE_EQ(result.finalCapital, 1000.0);
}

TEST(Backtester, NoSignalBeforeWarmup) {
    const auto candles = flatCandles({100, 100, 100, 100});
    ScriptedStrategy strategy({{0, Signal::BUY}}, /*warmupBars=*/2);
    const auto result = Backtester(frictionless()).run(candles, strategy);
    EXPECT_TRUE(result.trades.empty());
}

TEST(Backtester, SimulateOnlyTradesInsideTheRange) {
    const auto candles = flatCandles({100, 100, 100, 200, 200, 200});
    ScriptedStrategy strategy({{0, Signal::BUY}, {3, Signal::BUY}});
    strategy.prepare(candles);
    const Backtester backtester(frictionless());

    const auto test = backtester.simulate(candles, strategy, 3, 6);
    EXPECT_EQ(test.equityCurve.size(), 3U);
    ASSERT_EQ(test.trades.size(), 1U);
    EXPECT_EQ(test.trades[0].entryTimestamp, candles[4].timestamp);
    EXPECT_DOUBLE_EQ(test.buyHoldRoi, 0.0);
}

TEST(Backtester, ExposureAndBuyAndHold) {
    const auto candles = flatCandles({100, 100, 100, 100, 200});
    ScriptedStrategy strategy({{2, Signal::BUY}});
    const auto result = Backtester(frictionless()).run(candles, strategy);
    EXPECT_DOUBLE_EQ(result.exposure, 40.0); // bars 3 and 4 out of 5
    EXPECT_DOUBLE_EQ(result.buyHoldRoi, 100.0);
}

TEST(Backtester, RejectsInvalidConfigAndUnpreparedStrategy) {
    BacktestConfig config;
    config.initialCapital = 0.0;
    EXPECT_THROW(Backtester{config}, std::invalid_argument);

    const auto candles = flatCandles({100, 100});
    ScriptedStrategy strategy({});
    EXPECT_THROW(Backtester().simulate(candles, strategy), std::logic_error);
    EXPECT_THROW(Backtester().run({}, strategy), std::runtime_error);
}
