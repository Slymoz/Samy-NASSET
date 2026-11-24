#pragma once

#include "Strategy.hpp"
#include "Types.hpp"

#include <cstddef>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace test {

// Distinct, increasing, valid dates starting at 2024-01-01. Months are
// treated as 28 days long: the tests only need an ordered series.
inline std::string date(std::size_t index) {
    std::ostringstream out;
    out << std::setfill('0') << std::setw(4) << 2024 + index / (28 * 12) << "-" << std::setw(2) << (index / 28) % 12 + 1
        << "-" << std::setw(2) << index % 28 + 1;
    return out.str();
}

inline Candle candle(std::size_t day, double open, double high, double low, double close) {
    return Candle{date(day), open, high, low, close, 1000.0};
}

// Flat candles (open = high = low = close) from a list of prices.
inline std::vector<Candle> flatCandles(const std::vector<double> &prices) {
    std::vector<Candle> candles;
    for (std::size_t i = 0; i < prices.size(); ++i) {
        candles.push_back(candle(i, prices[i], prices[i], prices[i], prices[i]));
    }
    return candles;
}

// Emits predefined signals at given bars: lets the tests drive the engine
// precisely without depending on indicator logic.
class ScriptedStrategy : public Strategy {
  public:
    explicit ScriptedStrategy(std::map<std::size_t, Signal> signals, std::size_t warmupBars = 0)
        : m_signals(std::move(signals)),
          m_warmup(warmupBars) {}

    Signal onBar(const std::vector<Candle> & /*candles*/, std::size_t index, bool /*inPosition*/) const override {
        const auto it = m_signals.find(index);
        return it == m_signals.end() ? Signal::NONE : it->second;
    }

    std::size_t warmup() const override {
        return m_warmup;
    }

    std::string name() const override {
        return "Scripted";
    }

  protected:
    void compute(const std::vector<Candle> & /*candles*/) override {}

  private:
    std::map<std::size_t, Signal> m_signals;
    std::size_t m_warmup;
};

// Configuration without costs, to check prices exactly.
inline BacktestConfig frictionless(double capital = 1000.0) {
    BacktestConfig config;
    config.initialCapital = capital;
    config.feeRate = 0.0;
    config.slippageRate = 0.0;
    return config;
}

} // namespace test
