#pragma once

#include "Strategy.hpp"

#include <cstddef>
#include <string>
#include <vector>

enum class MaType { SMA, EMA };

// Trend following: buy when the short moving average crosses above the long
// one, sell when it crosses back below.
class MovingAverageCrossover : public Strategy {
  public:
    MovingAverageCrossover(std::size_t shortPeriod, std::size_t longPeriod, MaType type = MaType::SMA);

    Signal onBar(const std::vector<Candle> &candles, std::size_t index, bool inPosition) const override;
    std::size_t warmup() const override;
    std::string name() const override;

  protected:
    void compute(const std::vector<Candle> &candles) override;

  private:
    std::size_t m_shortPeriod;
    std::size_t m_longPeriod;
    MaType m_type;
    std::vector<double> m_shortMA;
    std::vector<double> m_longMA;
};
