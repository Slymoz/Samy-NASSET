#pragma once

#include "Strategy.hpp"
#include "Types.hpp"

#include <cstddef>
#include <vector>

// Event-driven, long-only simulation engine.
//
// Execution model, for each bar i:
//   1. the signal emitted at the close of bar i-1 is filled at the open of
//      bar i (no look-ahead), with slippage and fees;
//   2. stop-loss / take-profit are checked against the bar's low / high and
//      filled at the stop price, or at the open if the bar gapped through it.
//      When both levels are hit in the same bar, the stop-loss wins (we
//      cannot know the order of the moves, so assume the worst);
//   3. the equity is marked at the close;
//   4. the strategy sees the close of bar i and may emit a new signal.
// An open position is closed at the close of the last bar.
class Backtester {
  public:
    explicit Backtester(BacktestConfig config = {});

    // Prepare the strategy, then simulate the whole series.
    BacktestResult run(const std::vector<Candle> &candles, Strategy &strategy) const;

    // Simulate bars [from, to) with an already prepared strategy. Indicators
    // are computed on the full series, so the bars before `from` serve as
    // warm-up. `to == 0` means the end of the series. Thread-safe.
    BacktestResult simulate(const std::vector<Candle> &candles, const Strategy &strategy, std::size_t from = 0,
                            std::size_t to = 0) const;

    const BacktestConfig &config() const {
        return m_config;
    }

  private:
    BacktestConfig m_config;
};
