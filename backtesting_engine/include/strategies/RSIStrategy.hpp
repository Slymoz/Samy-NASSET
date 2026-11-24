#pragma once

#include "Strategy.hpp"

#include <cstddef>
#include <string>
#include <vector>

// Mean reversion: buy when the RSI leaves the oversold zone (crosses back
// above `oversold`), sell when it leaves the overbought zone.
class RSIStrategy : public Strategy {
  public:
    explicit RSIStrategy(std::size_t period = 14, double oversold = 30.0, double overbought = 70.0);

    Signal onBar(const std::vector<Candle> &candles, std::size_t index, bool inPosition) const override;
    std::size_t warmup() const override;
    std::string name() const override;

  protected:
    void compute(const std::vector<Candle> &candles) override;

  private:
    std::size_t m_period;
    double m_oversold;
    double m_overbought;
    std::vector<double> m_rsi;
};
