#include "Backtester.hpp"
#include "TestHelpers.hpp"
#include "strategies/MovingAverageCrossover.hpp"
#include "strategies/RSIStrategy.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

using test::flatCandles;

TEST(MovingAverageCrossover, BuysOnGoldenCrossAndSellsOnDeathCross) {
    // Falling, then rising (golden cross), then falling again (death cross).
    const auto candles = flatCandles({10, 9, 8, 7, 6, 7, 9, 12, 15, 14, 11, 8, 5, 4});
    MovingAverageCrossover strategy(2, 4);
    strategy.prepare(candles);

    std::size_t buys = 0;
    std::size_t sells = 0;
    bool inPosition = false;
    for (std::size_t i = strategy.warmup(); i < candles.size(); ++i) {
        const Signal signal = strategy.onBar(candles, i, inPosition);
        if (signal == Signal::BUY) {
            ++buys;
            inPosition = true;
        } else if (signal == Signal::SELL) {
            ++sells;
            inPosition = false;
        }
    }
    EXPECT_EQ(buys, 1U);
    EXPECT_EQ(sells, 1U);
}

TEST(MovingAverageCrossover, NoSellWithoutPositionAndNoBuyWhenInPosition) {
    const auto candles = flatCandles({10, 9, 8, 7, 6, 7, 9, 12, 15, 14, 11, 8, 5, 4});
    MovingAverageCrossover strategy(2, 4, MaType::EMA);
    strategy.prepare(candles);
    for (std::size_t i = strategy.warmup(); i < candles.size(); ++i) {
        EXPECT_NE(strategy.onBar(candles, i, false), Signal::SELL);
        EXPECT_NE(strategy.onBar(candles, i, true), Signal::BUY);
    }
}

TEST(MovingAverageCrossover, InvalidPeriodsThrow) {
    EXPECT_THROW(MovingAverageCrossover(50, 10), std::invalid_argument);
    EXPECT_THROW(MovingAverageCrossover(10, 10), std::invalid_argument);
    EXPECT_THROW(MovingAverageCrossover(0, 10), std::invalid_argument);
}

TEST(RSIStrategy, BuysWhenLeavingOversold) {
    // A steady fall pushes the RSI to 0, the rebound brings it back above 30.
    const auto candles = flatCandles({20, 19, 18, 17, 16, 15, 14, 16, 18, 20});
    RSIStrategy strategy(3, 30, 70);
    strategy.prepare(candles);

    bool bought = false;
    for (std::size_t i = strategy.warmup(); i < candles.size(); ++i) {
        bought = bought || strategy.onBar(candles, i, false) == Signal::BUY;
    }
    EXPECT_TRUE(bought);
}

TEST(RSIStrategy, InvalidThresholdsThrow) {
    EXPECT_THROW(RSIStrategy(14, 70, 30), std::invalid_argument);
    EXPECT_THROW(RSIStrategy(14, 0, 70), std::invalid_argument);
    EXPECT_THROW(RSIStrategy(0, 30, 70), std::invalid_argument);
}

TEST(Strategies, ShortSeriesDoesNotCrash) {
    // Fewer bars than the warm-up: no signal, no out-of-bounds access.
    const auto candles = flatCandles({10, 11, 12});
    MovingAverageCrossover ma(5, 20);
    RSIStrategy rsi(14);
    EXPECT_TRUE(Backtester().run(candles, ma).trades.empty());
    EXPECT_TRUE(Backtester().run(candles, rsi).trades.empty());
}
