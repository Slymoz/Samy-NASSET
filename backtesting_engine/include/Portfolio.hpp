#pragma once

#include "Types.hpp"
#include "Strategy.hpp"
#include <vector>
#include <memory>

class Portfolio {
  public:
    explicit Portfolio(double capital, double feeRate = 0.001);

    BacktestResult run(const std::vector<Candle> &candles, Strategy &strategy, bool silent = false);

    static void printReport(const BacktestResult &result);

  private:
    double m_initialCapital;
    double m_cash;
    double m_position;
    double m_feeRate;

    std::vector<double> m_portfolioValues;
    std::vector<TradeRecord> m_trades;
    TradeRecord m_pendingBuy;

    void executeOrder(const Order &order, double currentPrice, bool silent);
    double portfolioValue(double currentPrice) const;
    double computeMaxDrawdown() const;
};
