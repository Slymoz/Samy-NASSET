#include "Backtester.hpp"

#include "Metrics.hpp"

#include <algorithm>
#include <stdexcept>

const char *toString(ExitReason reason) {
    switch (reason) {
    case ExitReason::SIGNAL:
        return "signal";
    case ExitReason::STOP_LOSS:
        return "stop-loss";
    case ExitReason::TAKE_PROFIT:
        return "take-profit";
    case ExitReason::END_OF_DATA:
        return "end";
    }
    return "?";
}

namespace {

void validate(const BacktestConfig &config) {
    if (!(config.initialCapital > 0.0)) {
        throw std::invalid_argument("The initial capital must be > 0");
    }
    if (config.feeRate < 0.0 || config.feeRate >= 1.0) {
        throw std::invalid_argument("The fee rate must be in [0, 1)");
    }
    if (config.slippageRate < 0.0 || config.slippageRate >= 1.0) {
        throw std::invalid_argument("The slippage rate must be in [0, 1)");
    }
    if (config.stopLossPct < 0.0 || config.stopLossPct >= 100.0) {
        throw std::invalid_argument("The stop-loss must be in [0, 100) %");
    }
    if (config.takeProfitPct < 0.0) {
        throw std::invalid_argument("The take-profit must be >= 0 %");
    }
    if (!(config.periodsPerYear > 0.0)) {
        throw std::invalid_argument("The number of periods per year must be > 0");
    }
}

// Cash and position of a single long position, plus the trade being built.
class Account {
  public:
    Account(const BacktestConfig &config, double cash) : m_config(config), m_cash(cash) {}

    bool inPosition() const {
        return m_quantity > 0.0;
    }

    double entryPrice() const {
        return m_open.entryPrice;
    }

    double equity(double markPrice) const {
        return m_cash + m_quantity * markPrice;
    }

    double cash() const {
        return m_cash;
    }

    // Invest all the cash at `price` (before slippage).
    void buy(const Candle &bar, double price) {
        const double fill = price * (1.0 + m_config.slippageRate);
        const double quantity = m_cash / (fill * (1.0 + m_config.feeRate));
        const double fee = quantity * fill * m_config.feeRate;

        m_cash -= quantity * fill + fee;
        m_cash = std::max(0.0, m_cash); // absorb rounding residue
        m_quantity = quantity;

        m_open = TradeRecord{};
        m_open.entryTimestamp = bar.timestamp;
        m_open.entryPrice = fill;
        m_open.quantity = quantity;
        m_open.entryFee = fee;
    }

    // Sell the whole position at `price` (before slippage).
    TradeRecord sell(const Candle &bar, double price, ExitReason reason) {
        const double fill = price * (1.0 - m_config.slippageRate);
        const double revenue = m_quantity * fill;
        const double fee = revenue * m_config.feeRate;

        m_cash += revenue - fee;
        m_quantity = 0.0;

        TradeRecord trade = m_open;
        trade.exitTimestamp = bar.timestamp;
        trade.exitPrice = fill;
        trade.exitFee = fee;
        trade.exitReason = reason;
        const double invested = trade.entryPrice * trade.quantity + trade.entryFee;
        trade.pnl = (revenue - fee) - invested;
        trade.returnPct = invested > 0.0 ? trade.pnl / invested * 100.0 : 0.0;
        return trade;
    }

  private:
    const BacktestConfig &m_config;
    double m_cash;
    double m_quantity = 0.0;
    TradeRecord m_open;
};

} // namespace

Backtester::Backtester(BacktestConfig config) : m_config(config) {
    validate(m_config);
}

BacktestResult Backtester::run(const std::vector<Candle> &candles, Strategy &strategy) const {
    if (candles.empty()) {
        throw std::runtime_error("No data to simulate.");
    }
    strategy.prepare(candles);
    return simulate(candles, strategy);
}

BacktestResult Backtester::simulate(const std::vector<Candle> &candles, const Strategy &strategy, std::size_t from,
                                    std::size_t to) const {
    if (to == 0) {
        to = candles.size();
    }
    if (from >= to || to > candles.size()) {
        throw std::invalid_argument("Invalid simulation range.");
    }
    if (!strategy.isPreparedFor(candles)) {
        throw std::logic_error("The strategy must be prepared on these candles before simulating.");
    }

    const BacktestConfig &cfg = m_config;
    Account account(cfg, cfg.initialCapital);
    BacktestResult result;
    result.initialCapital = cfg.initialCapital;
    result.equityCurve.reserve(to - from);

    const std::size_t firstSignalBar = std::max(from, strategy.warmup());
    Signal pending = Signal::NONE;
    std::size_t barsInMarket = 0;

    for (std::size_t i = from; i < to; ++i) {
        const Candle &bar = candles[i];

        // 1. Fill the previous bar's signal at this bar's open.
        if (pending == Signal::BUY && !account.inPosition()) {
            account.buy(bar, bar.open);
        } else if (pending == Signal::SELL && account.inPosition()) {
            result.trades.push_back(account.sell(bar, bar.open, ExitReason::SIGNAL));
        }
        pending = Signal::NONE;

        // 2. Intra-bar risk management.
        if (account.inPosition()) {
            ++barsInMarket;
            const double entry = account.entryPrice();
            if (cfg.stopLossPct > 0.0) {
                const double stop = entry * (1.0 - cfg.stopLossPct / 100.0);
                if (bar.low <= stop) {
                    result.trades.push_back(account.sell(bar, std::min(bar.open, stop), ExitReason::STOP_LOSS));
                }
            }
            if (account.inPosition() && cfg.takeProfitPct > 0.0) {
                const double target = entry * (1.0 + cfg.takeProfitPct / 100.0);
                if (bar.high >= target) {
                    result.trades.push_back(account.sell(bar, std::max(bar.open, target), ExitReason::TAKE_PROFIT));
                }
            }
        }

        // 3. Mark to market.
        result.equityCurve.push_back({bar.timestamp, account.equity(bar.close)});

        // 4. New signal, only if there is a next bar to execute it.
        if (i >= firstSignalBar && i + 1 < to) {
            pending = strategy.onBar(candles, i, account.inPosition());
        }
    }

    const Candle &last = candles[to - 1];
    if (account.inPosition()) {
        result.trades.push_back(account.sell(last, last.close, ExitReason::END_OF_DATA));
        result.equityCurve.back().equity = account.cash();
    }

    std::vector<double> equity;
    equity.reserve(result.equityCurve.size());
    for (const auto &point : result.equityCurve) {
        equity.push_back(point.equity);
    }

    const auto bars = static_cast<double>(to - from);
    result.finalCapital = account.cash();
    result.roi = (result.finalCapital - cfg.initialCapital) / cfg.initialCapital * 100.0;
    result.cagr = Metrics::cagr(cfg.initialCapital, result.finalCapital, bars, cfg.periodsPerYear);
    result.maxDrawdown = Metrics::maxDrawdown(equity);
    const auto barReturns = Metrics::returns(equity);
    result.sharpe = Metrics::sharpe(barReturns, cfg.periodsPerYear);
    result.sortino = Metrics::sortino(barReturns, cfg.periodsPerYear);
    result.exposure = static_cast<double>(barsInMarket) / bars * 100.0;

    // Buy & hold over the same bars, with the same costs.
    const double entryCost = candles[from].open * (1.0 + cfg.slippageRate) * (1.0 + cfg.feeRate);
    const double exitValue = last.close * (1.0 - cfg.slippageRate) * (1.0 - cfg.feeRate);
    result.buyHoldRoi = (exitValue / entryCost - 1.0) * 100.0;

    Metrics::fillTradeStats(result);
    return result;
}
