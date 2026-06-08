#pragma once

#include <vector>
#include <string>
#include <functional>
#include <memory>
#include "Types.hpp"
#include "Portfolio.hpp"

struct OptimResult {
    std::string label;
    double roi;
    double maxDrawdown;
    double winRate;
    double profitFactor;
    int totalTrades;
    double stopLoss;
    double takeProfit;
};

class Optimizer {

  public:
    explicit Optimizer(const std::vector<Candle> &candles, double initialCapital = 10000.0, double feeRate = 0.001);

    std::vector<OptimResult> runMA(const std::vector<int> &shortPeriods, const std::vector<int> &longPeriods,
                                   const std::vector<double> &stopLosses, const std::vector<double> &takeProfits);

    std::vector<OptimResult> runRSI(const std::vector<int> &periods, const std::vector<double> &oversolds,
                                    const std::vector<double> &overboughts, const std::vector<double> &stopLosses,
                                    const std::vector<double> &takeProfits);

    static void printResults(const std::vector<OptimResult> &results, const std::string &stratName);

  private:
    const std::vector<Candle> &m_candles;
    double m_initialCapital;
    double m_feeRate;

    static double score(const OptimResult &r);
};
