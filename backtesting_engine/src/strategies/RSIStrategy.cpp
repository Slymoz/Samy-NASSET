#include "strategies/RSIStrategy.hpp"

#include "Indicators.hpp"

#include <sstream>
#include <stdexcept>

RSIStrategy::RSIStrategy(std::size_t period, double oversold, double overbought)
    : m_period(period),
      m_oversold(oversold),
      m_overbought(overbought) {
    if (period == 0) {
        throw std::invalid_argument("The RSI period must be > 0");
    }
    if (!(oversold > 0.0 && oversold < overbought && overbought < 100.0)) {
        throw std::invalid_argument("RSI thresholds must satisfy 0 < oversold < overbought < 100");
    }
}

void RSIStrategy::compute(const std::vector<Candle> &candles) {
    m_rsi = Indicators::RSI(closes(candles), m_period);
}

// The first RSI value is at index `period`; one more bar to detect a cross.
std::size_t RSIStrategy::warmup() const {
    return m_period + 1;
}

Signal RSIStrategy::onBar(const std::vector<Candle> & /*candles*/, std::size_t index, bool inPosition) const {
    const double rsiNow = m_rsi[index];
    const double rsiPrev = m_rsi[index - 1];

    if (!inPosition && rsiPrev < m_oversold && rsiNow >= m_oversold) {
        return Signal::BUY;
    }
    if (inPosition && rsiPrev > m_overbought && rsiNow <= m_overbought) {
        return Signal::SELL;
    }
    return Signal::NONE;
}

std::string RSIStrategy::name() const {
    std::ostringstream out;
    out << "RSI(" << m_period << " " << m_oversold << "/" << m_overbought << ")";
    return out.str();
}
