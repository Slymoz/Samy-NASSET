#include "Metrics.hpp"

#include <gtest/gtest.h>

#include <cmath>

namespace {

TradeRecord trade(double pnl) {
    TradeRecord t;
    t.pnl = pnl;
    return t;
}

} // namespace

TEST(Metrics, MaxDrawdownFromPeak) {
    EXPECT_DOUBLE_EQ(Metrics::maxDrawdown({100, 120, 90, 130, 117}), 25.0);
    EXPECT_DOUBLE_EQ(Metrics::maxDrawdown({100, 110, 120}), 0.0);
    EXPECT_DOUBLE_EQ(Metrics::maxDrawdown({}), 0.0);
}

TEST(Metrics, ProfitFactorWithoutLossesIsInfinite) {
    // Regression: it used to be 0, which ranked perfect strategies last.
    EXPECT_TRUE(std::isinf(Metrics::profitFactor({trade(10), trade(5)})));
    EXPECT_DOUBLE_EQ(Metrics::profitFactor({trade(30), trade(-10)}), 3.0);
    EXPECT_DOUBLE_EQ(Metrics::profitFactor({}), 0.0);
    EXPECT_DOUBLE_EQ(Metrics::profitFactor({trade(-5)}), 0.0);
}

TEST(Metrics, Returns) {
    const auto r = Metrics::returns({100, 110, 99});
    ASSERT_EQ(r.size(), 2U);
    EXPECT_NEAR(r[0], 0.10, 1e-12);
    EXPECT_NEAR(r[1], -0.10, 1e-12);
    EXPECT_TRUE(Metrics::returns({100}).empty());
}

TEST(Metrics, SharpeIsAnnualised) {
    const std::vector<double> r = {0.01, -0.01, 0.02, 0.0};
    // mean 0.005, squared deviations sum to 0.0005 -> sample std = sqrt(0.0005 / 3)
    const double expected = 0.005 / std::sqrt(0.0005 / 3.0) * std::sqrt(365.0);
    EXPECT_NEAR(Metrics::sharpe(r, 365.0), expected, 1e-9);
    EXPECT_DOUBLE_EQ(Metrics::sharpe({0.01, 0.01, 0.01}, 365.0), 0.0); // no volatility
}

TEST(Metrics, SortinoOnlyPenalisesDownside) {
    const std::vector<double> r = {0.02, -0.01, 0.03, -0.02};
    const double downside = std::sqrt((0.0001 + 0.0004) / 4.0);
    EXPECT_NEAR(Metrics::sortino(r, 252.0), 0.005 / downside * std::sqrt(252.0), 1e-9);
    EXPECT_TRUE(std::isinf(Metrics::sortino({0.01, 0.02}, 252.0)));
}

TEST(Metrics, Cagr) {
    EXPECT_NEAR(Metrics::cagr(100, 200, 365, 365), 100.0, 1e-9);    // doubled in 1 year
    EXPECT_NEAR(Metrics::cagr(100, 400, 730, 365), 100.0, 1e-9);    // x4 in 2 years
    EXPECT_NEAR(Metrics::cagr(100, 121, 365 * 2, 365), 10.0, 1e-9); // +10 %/year
    EXPECT_DOUBLE_EQ(Metrics::cagr(100, 0, 365, 365), 0.0);
}

TEST(Metrics, TradeStatistics) {
    BacktestResult result;
    result.trades = {trade(100), trade(-50), trade(200), trade(-25)};
    Metrics::fillTradeStats(result);
    EXPECT_EQ(result.totalTrades, 4);
    EXPECT_EQ(result.winningTrades, 2);
    EXPECT_EQ(result.losingTrades, 2);
    EXPECT_DOUBLE_EQ(result.winRate, 50.0);
    EXPECT_DOUBLE_EQ(result.totalPnl, 225.0);
    EXPECT_DOUBLE_EQ(result.avgWin, 150.0);
    EXPECT_DOUBLE_EQ(result.avgLoss, 37.5);
    EXPECT_DOUBLE_EQ(result.profitFactor, 4.0);
}
