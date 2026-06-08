#pragma once

#include <optional>
#include <iostream>
#include "../../include/Strategy.hpp"
#include "../../include/Indicators.hpp"

class MovingAverageCrossover : public Strategy {
  public:
    MovingAverageCrossover(int shortPeriod, int longPeriod, double feeRate = 0.001)
        : m_shortPeriod(shortPeriod),
          m_longPeriod(longPeriod),
          m_feeRate(feeRate) {}

    void prepare(const std::vector<Candle> &candles) override {
        std::vector<double> closes;
        closes.reserve(candles.size());
        for (const auto &c : candles) {
            closes.push_back(c.close);
        }

        m_shortMA = Indicators::SMA(closes, m_shortPeriod);
        m_longMA = Indicators::SMA(closes, m_longPeriod);
    }

    std::optional<Order> evaluate(const std::vector<Candle> &candles, int index, double currentCash,
                                  double currentPosition) override {
        if (index < m_longPeriod) {
            return std::nullopt;
        }

        auto riskOrder = checkRiskManagement(candles, index, currentPosition);
        if (riskOrder.has_value()) {
            return riskOrder;
        }

        double shortNow = m_shortMA[index];
        double shortPrev = m_shortMA[index - 1];
        double longNow = m_longMA[index];
        double longPrev = m_longMA[index - 1];

        if (shortPrev <= longPrev && shortNow > longNow && currentPosition == 0.0) {
            double price = candles[index].close;
            double quantity = (currentCash / price) / (1.0 + m_feeRate);
            recordEntry(price);
            return Order{candles[index].timestamp, OrderSide::BUY, price, quantity};
        }

        if (shortPrev >= longPrev && shortNow < longNow && currentPosition > 0.0) {
            recordExit();
            return Order{candles[index].timestamp, OrderSide::SELL, candles[index].close, currentPosition};
        }

        return std::nullopt;
    }

    std::string name() const override {
        std::string n =
            "MA Crossover (short=" + std::to_string(m_shortPeriod) + " / long=" + std::to_string(m_longPeriod) + ")";
        if (m_useStopLoss) {
            n += " [SL=" + std::to_string((int)m_stopLossPercent) + "%]";
        }
        if (m_useTakeProfit) {
            n += " [TP=" + std::to_string((int)m_takeProfitPercent) + "%]";
        }
        return n;
    }

  private:
    int m_shortPeriod;
    int m_longPeriod;
    double m_feeRate;
    std::vector<double> m_shortMA;
    std::vector<double> m_longMA;
};
