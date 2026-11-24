#pragma once

#include "Backtester.hpp"
#include "Types.hpp"
#include "strategies/MovingAverageCrossover.hpp"

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

struct OptimizerOptions {
    // Share of the bars used to choose the parameters (in-sample). The rest
    // is kept to evaluate them on unseen data (out-of-sample). 1 = no split.
    double trainRatio = 0.7;
    unsigned threads = 0; // 0 = all hardware threads
    int minTrades = 3;    // fewer trades than this in-sample = not ranked
};

struct OptimResult {
    std::string label;
    double stopLossPct = 0.0;
    double takeProfitPct = 0.0;
    double score = 0.0;                 // computed on the train period
    BacktestResult train;               // in-sample
    std::optional<BacktestResult> test; // out-of-sample, if trainRatio < 1
};

struct MaGrid {
    std::vector<std::size_t> shortPeriods;
    std::vector<std::size_t> longPeriods;
    std::vector<double> stopLosses;  // 0 = disabled
    std::vector<double> takeProfits; // 0 = disabled
    MaType type = MaType::SMA;
};

struct RsiGrid {
    std::vector<std::size_t> periods;
    std::vector<double> oversolds;
    std::vector<double> overboughts;
    std::vector<double> stopLosses;
    std::vector<double> takeProfits;
};

// Grid search over strategy parameters, run in parallel. Results are sorted
// by in-sample score (best first) and do not depend on the thread count.
class Optimizer {
  public:
    using StrategyFactory = std::function<std::unique_ptr<Strategy>()>;

    Optimizer(std::vector<Candle> candles, BacktestConfig baseConfig, OptimizerOptions options = {});

    std::vector<OptimResult> runMA(const MaGrid &grid) const;
    std::vector<OptimResult> runRSI(const RsiGrid &grid) const;

    // Every factory is combined with every stop-loss / take-profit value
    // (an empty list means "disabled").
    std::vector<OptimResult> run(const std::vector<StrategyFactory> &factories, std::vector<double> stopLosses,
                                 std::vector<double> takeProfits) const;

    // First bar of the out-of-sample period (== number of bars if none).
    std::size_t splitIndex() const {
        return m_split;
    }

    const std::vector<Candle> &candles() const {
        return m_candles;
    }

    // Risk-adjusted ranking score: ROI x PF / (1 + MaxDD) for profitable
    // runs (PF capped so a lucky trade cannot dominate), the ROI itself for
    // losing ones, and a large negative value under `minTrades` trades.
    static double score(const BacktestResult &result, int minTrades);

  private:
    std::vector<Candle> m_candles; // owned: no dangling reference
    BacktestConfig m_baseConfig;
    OptimizerOptions m_options;
    std::size_t m_split;
};
