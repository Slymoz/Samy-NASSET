#pragma once

#include "../../include/Strategy.hpp"
#include "../../include/Indicators.hpp"
#include <optional>

class RSIStrategy : public Strategy {
  public:
    explicit RSIStrategy(int period = 14, double oversold = 30.0, double overbought = 70.0, double feeRate = 0.001)
        : m_period(period),
          m_oversold(oversold),
          m_overbought(overbought),
          m_feeRate(feeRate) {}

    void prepare(const std::vector<Candle> &candles) override {
        std::vector<double> closes;
        closes.reserve(candles.size());
        for (const auto &c : candles) {
            closes.push_back(c.close);
        }

        m_rsi = Indicators::RSI(closes, m_period);
    }

    std::optional<Order> evaluate(const std::vector<Candle> &candles, int index, double currentCash,
                                  double currentPosition) override {
        if (index < m_period + 1) {
            return std::nullopt;
        }

        auto riskOrder = checkRiskManagement(candles, index, currentPosition);
        if (riskOrder.has_value()) {
            return riskOrder;
        }

        double rsiNow = m_rsi[index];
        double rsiPrev = m_rsi[index - 1];

        if (rsiPrev < m_oversold && rsiNow >= m_oversold && currentPosition == 0.0) {
            double price = candles[index].close;
            double quantity = (currentCash / price) / (1.0 + m_feeRate);
            recordEntry(price);
            return Order{candles[index].timestamp, OrderSide::BUY, price, quantity};
        }

        if (rsiPrev > m_overbought && rsiNow <= m_overbought && currentPosition > 0.0) {
            recordExit();
            return Order{candles[index].timestamp, OrderSide::SELL, candles[index].close, currentPosition};
        }

        return std::nullopt;
    }

    std::string name() const override {
        std::string n = "RSI Strategy (period=" + std::to_string(m_period) + ")";
        if (m_useStopLoss) {
            n += " [SL=" + std::to_string((int)m_stopLossPercent) + "%]";
        }
        if (m_useTakeProfit) {
            n += " [TP=" + std::to_string((int)m_takeProfitPercent) + "%]";
        }
        return n;
    }

  private:
    int m_period;
    double m_oversold;
    double m_overbought;
    double m_feeRate;
    std::vector<double> m_rsi;
};
