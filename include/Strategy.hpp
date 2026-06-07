#pragma once

#include "Types.hpp"
#include <vector>
#include <string>
#include <optional>

class Strategy {
public:
    virtual ~Strategy() = default;

    virtual void prepare(const std::vector<Candle>& candles) = 0;

    virtual std::optional<Order> evaluate(
        const std::vector<Candle>& candles,
        int                        index,
        double                     currentCash,
        double                     currentPosition
    ) = 0;

    virtual std::string name() const = 0;

    // ✅ Stop-loss configurable (par défaut désactivé)
    void setStopLoss(double percent) { m_stopLossPercent = percent; m_useStopLoss = true; }
    void setTakeProfit(double percent) { m_takeProfitPercent = percent; m_useTakeProfit = true; }

protected:
    // ✅ Gestion de l'entrée pour le stop-loss
    void recordEntry(double price) { m_entryPrice = price; m_inPosition = true; }
    void recordExit() { m_entryPrice = 0.0; m_inPosition = false; }

    // ✅ Vérifie si le stop-loss ou take-profit est touché
    std::optional<Order> checkRiskManagement(
        const std::vector<Candle>& candles,
        int                        index,
        double                     currentPosition
    ) {
        if (!m_inPosition || currentPosition <= 0.0)
            return std::nullopt;

        double currentPrice = candles[index].close;
        double pnlPercent   = (currentPrice - m_entryPrice) / m_entryPrice * 100.0;

        // ✅ Aucun cout ici — Portfolio affiche le [SELL] de toute façon
        if (m_useStopLoss && pnlPercent <= -m_stopLossPercent) {
            recordExit();
            return Order{
                candles[index].timestamp,
                OrderSide::SELL,
                currentPrice,
                currentPosition
            };
        }

        if (m_useTakeProfit && pnlPercent >= m_takeProfitPercent) {
            recordExit();
            return Order{
                candles[index].timestamp,
                OrderSide::SELL,
                currentPrice,
                currentPosition
            };
        }

        return std::nullopt;
    }

    double m_entryPrice       = 0.0;
    bool   m_inPosition       = false;
    bool   m_useStopLoss      = false;
    bool   m_useTakeProfit    = false;
    double m_stopLossPercent  = 5.0;   // défaut : -5%
    double m_takeProfitPercent = 15.0;  // défaut : +15%
};
