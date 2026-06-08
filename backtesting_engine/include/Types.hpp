#pragma once

#include <string>

struct Candle {
    std::string timestamp;
    double open;
    double high;
    double low;
    double close;
    double volume;
};

enum class OrderSide { BUY, SELL };

struct Order {
    std::string timestamp;
    OrderSide side;
    double price;
    double quantity;
};

struct TradeRecord {
    std::string buyTimestamp;
    std::string sellTimestamp;
    double buyPrice;
    double sellPrice;
    double quantity;
    double buyFee;
    double sellFee;
    double pnl;
    bool isWin;
};

struct BacktestResult {
    double initialCapital;
    double finalCapital;
    double roi;
    double maxDrawdown;
    double winRate;
    int totalTrades;
    int winningTrades;
    int losingTrades;
    double totalPnl;
    double avgWin;
    double avgLoss;
    double profitFactor;
};
