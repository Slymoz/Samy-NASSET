#pragma once

#include <string>

struct Candle {
    std::string timestamp;
    double      open;
    double      high;
    double      low;
    double      close;
    double      volume;
};

enum class OrderSide {
    BUY,
    SELL
};

struct Order {
    std::string timestamp;
    OrderSide   side;
    double      price;
    double      quantity;
};

// ✅ Nouveau : enregistrement complet d'un trade bouclé
struct TradeRecord {
    std::string buyTimestamp;
    std::string sellTimestamp;
    double      buyPrice;
    double      sellPrice;
    double      quantity;
    double      buyFee;
    double      sellFee;
    double      pnl;        // profit/perte net après frais
    bool        isWin;
};

struct BacktestResult {
    double initialCapital;
    double finalCapital;
    double roi;
    double maxDrawdown;
    double winRate;
    int    totalTrades;
    int    winningTrades;
    int    losingTrades;
    double totalPnl;        // ✅ P&L cumulé net
    double avgWin;          // ✅ gain moyen sur trades gagnants
    double avgLoss;         // ✅ perte moyenne sur trades perdants
    double profitFactor;    // ✅ ratio gains bruts / pertes brutes
};
