#pragma once

#include "Types.hpp"

#include <cstddef>
#include <string>
#include <vector>

// A trading strategy only decides *when* to be in the market. Position
// sizing, fees, slippage, stop-loss and take-profit are handled by the
// Backtester, so a strategy holds no trading state and can be shared by
// several simulations running in parallel once it is prepared.
class Strategy {
  public:
    virtual ~Strategy() = default;

    // Precompute the indicators for the whole series. Indicators must be
    // causal (value at index i only depends on bars <= i) to avoid look-ahead.
    void prepare(const std::vector<Candle> &candles) {
        compute(candles);
        m_preparedBars = candles.size();
    }

    bool isPreparedFor(const std::vector<Candle> &candles) const {
        return m_preparedBars == candles.size() && !candles.empty();
    }

    // Called at the close of bar `index` (index >= warmup()). The returned
    // signal is executed by the engine at the open of bar `index + 1`.
    virtual Signal onBar(const std::vector<Candle> &candles, std::size_t index, bool inPosition) const = 0;

    // First bar index at which onBar() may be called.
    virtual std::size_t warmup() const = 0;

    virtual std::string name() const = 0;

  protected:
    virtual void compute(const std::vector<Candle> &candles) = 0;

    static std::vector<double> closes(const std::vector<Candle> &candles) {
        std::vector<double> values;
        values.reserve(candles.size());
        for (const auto &candle : candles) {
            values.push_back(candle.close);
        }
        return values;
    }

  private:
    std::size_t m_preparedBars = 0;
};
