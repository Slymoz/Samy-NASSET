#pragma once

#include <cstddef>
#include <string>
#include <vector>

struct Candle {
    std::string timestamp;
    double open = 0.0;
    double high = 0.0;
    double low = 0.0;
    double close = 0.0;
    double volume = 0.0;
};

// What a strategy asks for at the close of a bar. The engine executes it at
// the open of the next bar, so a strategy can never trade on a price it has
// not seen yet.
enum class Signal { NONE, BUY, SELL };

enum class ExitReason { SIGNAL, STOP_LOSS, TAKE_PROFIT, END_OF_DATA };

struct TradeRecord {
    std::string entryTimestamp;
    std::string exitTimestamp;
    double entryPrice = 0.0; // fill price, slippage included
    double exitPrice = 0.0;  // fill price, slippage included
    double quantity = 0.0;
    double entryFee = 0.0;
    double exitFee = 0.0;
    double pnl = 0.0;       // net of fees
    double returnPct = 0.0; // pnl relative to the capital invested, in %
    ExitReason exitReason = ExitReason::SIGNAL;
};

struct EquityPoint {
    std::string timestamp;
    double equity = 0.0;
};

// Everything that describes how a backtest is simulated (not what is traded).
// Percentages are expressed in percent (5.0 = 5 %), rates as fractions.
struct BacktestConfig {
    double initialCapital = 10000.0;
    double feeRate = 0.001;        // 0.1 % per side
    double slippageRate = 0.0005;  // 0.05 % adverse price move per fill
    double stopLossPct = 0.0;      // 0 = disabled
    double takeProfitPct = 0.0;    // 0 = disabled
    double periodsPerYear = 365.0; // bars per year, used to annualise metrics
};

struct BacktestResult {
    double initialCapital = 0.0;
    double finalCapital = 0.0;
    double roi = 0.0;         // %
    double cagr = 0.0;        // %
    double maxDrawdown = 0.0; // %
    double sharpe = 0.0;      // annualised, risk-free rate = 0
    double sortino = 0.0;     // annualised, risk-free rate = 0
    double exposure = 0.0;    // % of bars spent in the market
    double buyHoldRoi = 0.0;  // % over the same period, fees and slippage included
    double winRate = 0.0;     // %
    int totalTrades = 0;
    int winningTrades = 0;
    int losingTrades = 0;
    double totalPnl = 0.0;
    double avgWin = 0.0;
    double avgLoss = 0.0;
    double profitFactor = 0.0; // +inf when there are wins and no losses

    std::vector<TradeRecord> trades;
    std::vector<EquityPoint> equityCurve;
};

const char *toString(ExitReason reason);
