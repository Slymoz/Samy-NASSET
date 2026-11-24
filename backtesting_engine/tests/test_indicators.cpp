#include "Indicators.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>

using Indicators::EMA;
using Indicators::RSI;
using Indicators::SMA;

TEST(SMA, ComputesRollingMean) {
    const auto sma = SMA({1, 2, 3, 4, 5}, 3);
    ASSERT_EQ(sma.size(), 5U);
    EXPECT_TRUE(std::isnan(sma[0]));
    EXPECT_TRUE(std::isnan(sma[1]));
    EXPECT_DOUBLE_EQ(sma[2], 2.0);
    EXPECT_DOUBLE_EQ(sma[3], 3.0);
    EXPECT_DOUBLE_EQ(sma[4], 4.0);
}

TEST(SMA, PeriodOneIsThePriceItself) {
    const auto sma = SMA({3, 7, 5}, 1);
    EXPECT_DOUBLE_EQ(sma[0], 3.0);
    EXPECT_DOUBLE_EQ(sma[2], 5.0);
}

TEST(SMA, PeriodLongerThanInputGivesOnlyNaN) {
    for (double value : SMA({1, 2}, 5)) {
        EXPECT_TRUE(std::isnan(value));
    }
}

TEST(EMA, SeededWithSmaThenSmoothed) {
    // period 3 -> multiplier 0.5, seed = SMA(1, 2, 3) = 2
    const auto ema = EMA({1, 2, 3, 4, 5}, 3);
    EXPECT_TRUE(std::isnan(ema[1]));
    EXPECT_DOUBLE_EQ(ema[2], 2.0);
    EXPECT_DOUBLE_EQ(ema[3], 3.0); // (4 - 2) * 0.5 + 2
    EXPECT_DOUBLE_EQ(ema[4], 4.0); // (5 - 3) * 0.5 + 3
}

TEST(EMA, InputShorterThanPeriodDoesNotOverflow) {
    const auto ema = EMA({1, 2}, 10);
    ASSERT_EQ(ema.size(), 2U);
    EXPECT_TRUE(std::isnan(ema[0]));
    EXPECT_TRUE(std::isnan(ema[1]));
    EXPECT_TRUE(EMA({}, 3).empty());
}

TEST(RSI, WilderSmoothingKnownValues) {
    // changes: +1, -1, +1, -1 ; period 2
    const auto rsi = RSI({1, 2, 1, 2, 1}, 2);
    EXPECT_TRUE(std::isnan(rsi[1]));
    EXPECT_DOUBLE_EQ(rsi[2], 50.0); // gain 0.5 / loss 0.5
    EXPECT_DOUBLE_EQ(rsi[3], 75.0); // gain 0.75 / loss 0.25
    EXPECT_DOUBLE_EQ(rsi[4], 37.5); // gain 0.375 / loss 0.625
}

TEST(RSI, OnlyGainsIs100AndFlatIs50) {
    const auto rising = RSI({1, 2, 3, 4, 5}, 2);
    EXPECT_DOUBLE_EQ(rising[4], 100.0);
    const auto flat = RSI({5, 5, 5, 5}, 2);
    EXPECT_DOUBLE_EQ(flat[3], 50.0);
}

TEST(RSI, InputNotLongerThanPeriodDoesNotOverflow) {
    const auto rsi = RSI({1, 2, 3}, 3);
    ASSERT_EQ(rsi.size(), 3U);
    for (double value : rsi) {
        EXPECT_TRUE(std::isnan(value));
    }
}

TEST(Indicators, ZeroPeriodThrows) {
    EXPECT_THROW(SMA({1, 2}, 0), std::invalid_argument);
    EXPECT_THROW(EMA({1, 2}, 0), std::invalid_argument);
    EXPECT_THROW(RSI({1, 2}, 0), std::invalid_argument);
}
