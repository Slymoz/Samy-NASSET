#include "strategies/MovingAverageCrossover.hpp"

#include "Indicators.hpp"

#include <stdexcept>

MovingAverageCrossover::MovingAverageCrossover(std::size_t shortPeriod, std::size_t longPeriod, MaType type)
    : m_shortPeriod(shortPeriod),
      m_longPeriod(longPeriod),
      m_type(type) {
    if (shortPeriod == 0 || longPeriod == 0) {
        throw std::invalid_argument("MA periods must be > 0");
    }
    if (shortPeriod >= longPeriod) {
        throw std::invalid_argument("The short MA period must be smaller than the long one");
    }
}

void MovingAverageCrossover::compute(const std::vector<Candle> &candles) {
    const auto prices = closes(candles);
    if (m_type == MaType::EMA) {
        m_shortMA = Indicators::EMA(prices, m_shortPeriod);
        m_longMA = Indicators::EMA(prices, m_longPeriod);
    } else {
        m_shortMA = Indicators::SMA(prices, m_shortPeriod);
        m_longMA = Indicators::SMA(prices, m_longPeriod);
    }
}

// Both averages must exist on the previous bar to detect a crossover.
std::size_t MovingAverageCrossover::warmup() const {
    return m_longPeriod;
}

Signal MovingAverageCrossover::onBar(const std::vector<Candle> & /*candles*/, std::size_t index,
                                     bool inPosition) const {
    const double shortNow = m_shortMA[index];
    const double shortPrev = m_shortMA[index - 1];
    const double longNow = m_longMA[index];
    const double longPrev = m_longMA[index - 1];

    if (!inPosition && shortPrev <= longPrev && shortNow > longNow) {
        return Signal::BUY;
    }
    if (inPosition && shortPrev >= longPrev && shortNow < longNow) {
        return Signal::SELL;
    }
    return Signal::NONE;
}

std::string MovingAverageCrossover::name() const {
    const char *kind = m_type == MaType::EMA ? "EMA" : "MA";
    return std::string(kind) + " Crossover (" + std::to_string(m_shortPeriod) + "/" + std::to_string(m_longPeriod) +
           ")";
}
