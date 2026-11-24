#include "Optimizer.hpp"
#include "TestHelpers.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <stdexcept>

namespace {

// A noisy uptrend with pullbacks: enough crossovers to produce trades.
std::vector<Candle> syntheticSeries(std::size_t bars) {
    std::vector<double> prices;
    for (std::size_t i = 0; i < bars; ++i) {
        const auto x = static_cast<double>(i);
        prices.push_back(100.0 + 0.2 * x + 8.0 * std::sin(x / 6.0) + 3.0 * std::sin(x / 1.7));
    }
    return test::flatCandles(prices);
}

MaGrid smallGrid() {
    MaGrid grid;
    grid.shortPeriods = {3, 5, 20};
    grid.longPeriods = {10, 20};
    grid.stopLosses = {0.0, 5.0};
    grid.takeProfits = {0.0, 10.0};
    return grid;
}

double testRoi(const OptimResult &r) {
    return r.test ? r.test->roi : std::nan("");
}

BacktestResult resultWith(double roi, double pf, double dd, int trades) {
    BacktestResult r;
    r.roi = roi;
    r.profitFactor = pf;
    r.maxDrawdown = dd;
    r.totalTrades = trades;
    return r;
}

} // namespace

TEST(Optimizer, Score) {
    EXPECT_DOUBLE_EQ(Optimizer::score(resultWith(20, 2, 9, 5), 3), 4.0); // 20 * 2 / 10
    // An infinite profit factor is capped instead of dominating the ranking.
    const double capped = Optimizer::score(resultWith(20, std::numeric_limits<double>::infinity(), 9, 5), 3);
    EXPECT_DOUBLE_EQ(capped, 20.0);
    // Losing runs are ranked by ROI, below every profitable one.
    EXPECT_DOUBLE_EQ(Optimizer::score(resultWith(-15, 0.2, 30, 5), 3), -15.0);
    EXPECT_LT(Optimizer::score(resultWith(50, 3, 5, 2), 3), -1e6); // too few trades
}

TEST(Optimizer, TestsEveryValidCombinationSortedByScore) {
    const Optimizer optimizer(syntheticSeries(300), test::frictionless(), {0.7, 2, 1});
    const auto results = optimizer.runMA(smallGrid());

    // (3,10) (3,20) (5,10) (5,20) are valid, (20,*) is not -> 4 x 2 SL x 2 TP.
    ASSERT_EQ(results.size(), 16U);
    for (std::size_t i = 1; i < results.size(); ++i) {
        EXPECT_GE(results[i - 1].score, results[i].score);
    }
    for (const auto &r : results) {
        ASSERT_TRUE(r.test.has_value());
        EXPECT_TRUE(r.train.equityCurve.empty()); // trimmed to save memory
    }
    EXPECT_EQ(optimizer.splitIndex(), 210U);
}

TEST(Optimizer, ResultsDoNotDependOnThreadCount) {
    const auto candles = syntheticSeries(300);
    const auto single = Optimizer(candles, test::frictionless(), {0.7, 1, 1}).runMA(smallGrid());
    const auto multi = Optimizer(candles, test::frictionless(), {0.7, 8, 1}).runMA(smallGrid());

    ASSERT_EQ(single.size(), multi.size());
    for (std::size_t i = 0; i < single.size(); ++i) {
        EXPECT_EQ(single[i].label, multi[i].label);
        EXPECT_DOUBLE_EQ(single[i].train.roi, multi[i].train.roi);
        ASSERT_TRUE(single[i].test && multi[i].test);
        EXPECT_DOUBLE_EQ(testRoi(single[i]), testRoi(multi[i]));
    }
}

TEST(Optimizer, NoTestPeriodWhenTrainRatioIsOne) {
    const Optimizer optimizer(syntheticSeries(200), test::frictionless(), {1.0, 2, 1});
    const auto results = optimizer.runRSI({{7, 14}, {30}, {70}, {}, {}});
    ASSERT_EQ(results.size(), 2U);
    EXPECT_FALSE(results[0].test.has_value());
}

TEST(Optimizer, InvalidOptionsThrow) {
    EXPECT_THROW(Optimizer({}, {}), std::invalid_argument);
    EXPECT_THROW(Optimizer(syntheticSeries(10), {}, {0.0, 1, 1}), std::invalid_argument);
}
