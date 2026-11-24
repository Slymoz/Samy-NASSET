#include "Metrics.hpp"

#include <cmath>
#include <limits>

namespace Metrics {

double maxDrawdown(const std::vector<double> &equity) {
    double maxDD = 0.0;
    double peak = 0.0;

    for (double value : equity) {
        if (value > peak) {
            peak = value;
        }
        if (peak > 0.0) {
            const double drawdown = (peak - value) / peak * 100.0;
            if (drawdown > maxDD) {
                maxDD = drawdown;
            }
        }
    }
    return maxDD;
}

std::vector<double> returns(const std::vector<double> &equity) {
    std::vector<double> result;
    if (equity.size() < 2) {
        return result;
    }
    result.reserve(equity.size() - 1);
    for (std::size_t i = 1; i < equity.size(); ++i) {
        result.push_back(equity[i - 1] > 0.0 ? equity[i] / equity[i - 1] - 1.0 : 0.0);
    }
    return result;
}

namespace {

double mean(const std::vector<double> &values) {
    double sum = 0.0;
    for (double v : values) {
        sum += v;
    }
    return sum / static_cast<double>(values.size());
}

} // namespace

double sharpe(const std::vector<double> &barReturns, double periodsPerYear) {
    if (barReturns.size() < 2) {
        return 0.0;
    }
    const double avg = mean(barReturns);
    double variance = 0.0;
    for (double r : barReturns) {
        variance += (r - avg) * (r - avg);
    }
    variance /= static_cast<double>(barReturns.size() - 1);
    const double stddev = std::sqrt(variance);
    if (stddev == 0.0) {
        return 0.0;
    }
    return avg / stddev * std::sqrt(periodsPerYear);
}

double sortino(const std::vector<double> &barReturns, double periodsPerYear) {
    if (barReturns.size() < 2) {
        return 0.0;
    }
    const double avg = mean(barReturns);
    double downside = 0.0;
    for (double r : barReturns) {
        if (r < 0.0) {
            downside += r * r;
        }
    }
    downside = std::sqrt(downside / static_cast<double>(barReturns.size()));
    if (downside == 0.0) {
        return avg > 0.0 ? std::numeric_limits<double>::infinity() : 0.0;
    }
    return avg / downside * std::sqrt(periodsPerYear);
}

double cagr(double initial, double final, double bars, double periodsPerYear) {
    if (initial <= 0.0 || final <= 0.0 || bars <= 0.0 || periodsPerYear <= 0.0) {
        return 0.0;
    }
    const double years = bars / periodsPerYear;
    return (std::pow(final / initial, 1.0 / years) - 1.0) * 100.0;
}

double profitFactor(const std::vector<TradeRecord> &trades) {
    double grossWin = 0.0;
    double grossLoss = 0.0;
    for (const auto &trade : trades) {
        if (trade.pnl > 0.0) {
            grossWin += trade.pnl;
        } else {
            grossLoss -= trade.pnl;
        }
    }
    if (grossLoss > 0.0) {
        return grossWin / grossLoss;
    }
    return grossWin > 0.0 ? std::numeric_limits<double>::infinity() : 0.0;
}

void fillTradeStats(BacktestResult &result) {
    double grossWin = 0.0;
    double grossLoss = 0.0;
    result.totalTrades = static_cast<int>(result.trades.size());
    result.winningTrades = 0;
    result.losingTrades = 0;
    result.totalPnl = 0.0;

    for (const auto &trade : result.trades) {
        result.totalPnl += trade.pnl;
        if (trade.pnl > 0.0) {
            ++result.winningTrades;
            grossWin += trade.pnl;
        } else {
            ++result.losingTrades;
            grossLoss -= trade.pnl;
        }
    }

    result.winRate =
        result.totalTrades > 0 ? static_cast<double>(result.winningTrades) / result.totalTrades * 100.0 : 0.0;
    result.avgWin = result.winningTrades > 0 ? grossWin / result.winningTrades : 0.0;
    result.avgLoss = result.losingTrades > 0 ? grossLoss / result.losingTrades : 0.0;
    result.profitFactor = profitFactor(result.trades);
}

} // namespace Metrics
