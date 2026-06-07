#include "../include/Portfolio.hpp"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <stdexcept>

Portfolio::Portfolio(double capital, double feeRate)
    : m_initialCapital(capital)
    , m_cash(capital)
    , m_position(0.0)
    , m_feeRate(feeRate)
{}

void Portfolio::executeOrder(const Order& order, double currentPrice, bool silent) {
    constexpr double EPSILON = 1e-9;
    double fee = 0.0;

    if (order.side == OrderSide::BUY) {
        double cost = order.price * order.quantity;
        fee         = cost * m_feeRate;

        if (cost + fee > m_cash + EPSILON) {
            if (!silent)
                std::cerr << "  [SKIP BUY] Fonds insuffisants : besoin="
                          << std::fixed << std::setprecision(4) << (cost + fee)
                          << " disponible=" << m_cash << "\n";
            return;
        }

        m_cash     = std::max(0.0, m_cash - (cost + fee));
        m_position += order.quantity;

        m_pendingBuy = {
            order.timestamp, "", order.price, 0.0,
            order.quantity, fee, 0.0, 0.0, false
        };

        if (!silent)
            std::cout << "  [BUY]  " << order.timestamp
                      << " | Prix: "  << std::fixed << std::setprecision(2) << order.price
                      << " | Qté: "   << std::setprecision(6) << order.quantity
                      << " | Frais: " << std::setprecision(4) << fee << "\n";

    } else if (order.side == OrderSide::SELL) {
        if (order.quantity > m_position + EPSILON) {
            if (!silent)
                std::cerr << "  [SKIP SELL] Position insuffisante.\n";
            return;
        }

        double actualQty = std::min(order.quantity, m_position);
        double revenue   = order.price * actualQty;
        fee              = revenue * m_feeRate;

        m_cash     += (revenue - fee);
        m_position  = std::max(0.0, m_position - actualQty);

        double costBasis = m_pendingBuy.buyPrice * actualQty + m_pendingBuy.buyFee;
        double proceeds  = revenue - fee;
        double pnl       = proceeds - costBasis;

        TradeRecord record = {
            m_pendingBuy.buyTimestamp, order.timestamp,
            m_pendingBuy.buyPrice, order.price,
            actualQty, m_pendingBuy.buyFee, fee,
            pnl, pnl > 0.0
        };

        m_trades.push_back(record);

        if (!silent)
            std::cout << "  [SELL] " << order.timestamp
                      << " | Prix: "  << std::fixed << std::setprecision(2) << order.price
                      << " | Qté: "   << std::setprecision(6) << actualQty
                      << " | Frais: " << std::setprecision(4) << fee
                      << " | PnL: "   << std::setprecision(2)
                      << (pnl >= 0 ? "+" : "") << pnl << "$\n";
    }
}

double Portfolio::portfolioValue(double currentPrice) const {
    return m_cash + m_position * currentPrice;
}

double Portfolio::computeMaxDrawdown() const {
    double maxDD = 0.0;
    double peak  = m_portfolioValues.empty() ? 0.0 : m_portfolioValues[0];

    for (double val : m_portfolioValues) {
        if (val > peak) peak = val;
        double dd = (peak - val) / peak * 100.0;
        if (dd > maxDD) maxDD = dd;
    }
    return maxDD;
}

BacktestResult Portfolio::run(
    const std::vector<Candle>& candles,
    Strategy&                  strategy,
    bool                       silent       // ✅
) {
    if (candles.empty())
        throw std::runtime_error("Aucune donnée à simuler.");

    m_cash     = m_initialCapital;
    m_position = 0.0;
    m_portfolioValues.clear();
    m_portfolioValues.reserve(candles.size());
    m_trades.clear();
    m_pendingBuy = {};

    strategy.prepare(candles);

    if (!silent)
        std::cout << "\n=== Simulation : " << strategy.name() << " ===\n";

    for (int i = 0; i < static_cast<int>(candles.size()); ++i) {
        auto optOrder = strategy.evaluate(candles, i, m_cash, m_position);

        if (optOrder.has_value()) {
            try {
                executeOrder(optOrder.value(), candles[i].close, silent);  // ✅
            } catch (const std::exception& e) {
                if (!silent)
                    std::cerr << "  [ERREUR ordre] " << e.what() << "\n";
            }
        }

        m_portfolioValues.push_back(portfolioValue(candles[i].close));
    }

    if (m_position > 0.0) {
        double finalPrice = candles.back().close;
        double revenue    = m_position * finalPrice;
        double fee        = revenue * m_feeRate;

        double costBasis = m_pendingBuy.buyPrice * m_position + m_pendingBuy.buyFee;
        double proceeds  = revenue - fee;
        double pnl       = proceeds - costBasis;

        m_trades.push_back({
            m_pendingBuy.buyTimestamp, candles.back().timestamp,
            m_pendingBuy.buyPrice, finalPrice,
            m_position, m_pendingBuy.buyFee, fee,
            pnl, pnl > 0.0
        });

        if (!silent)
            std::cout << "  [CLOSE] Fermeture position à "
                      << std::fixed << std::setprecision(2) << finalPrice
                      << " | PnL: " << (pnl >= 0 ? "+" : "") << pnl << "$\n";

        m_cash    += (revenue - fee);
        m_position = 0.0;
    }

    int    totalTrades   = static_cast<int>(m_trades.size());
    int    winningTrades = 0;
    int    losingTrades  = 0;
    double totalPnl      = 0.0;
    double grossWin      = 0.0;
    double grossLoss     = 0.0;

    for (const auto& t : m_trades) {
        totalPnl += t.pnl;
        if (t.isWin) { ++winningTrades; grossWin += t.pnl; }
        else         { ++losingTrades;  grossLoss += std::abs(t.pnl); }
    }

    double avgWin       = (winningTrades > 0) ? grossWin  / winningTrades : 0.0;
    double avgLoss      = (losingTrades  > 0) ? grossLoss / losingTrades  : 0.0;
    double profitFactor = (grossLoss > 0.0)   ? grossWin  / grossLoss     : 0.0;

    BacktestResult result;
    result.initialCapital = m_initialCapital;
    result.finalCapital   = m_cash;
    result.roi            = (m_cash - m_initialCapital) / m_initialCapital * 100.0;
    result.maxDrawdown    = computeMaxDrawdown();
    result.totalTrades    = totalTrades;
    result.winningTrades  = winningTrades;
    result.losingTrades   = losingTrades;
    result.winRate        = (totalTrades > 0)
                          ? static_cast<double>(winningTrades) / totalTrades * 100.0
                          : 0.0;
    result.totalPnl       = totalPnl;
    result.avgWin         = avgWin;
    result.avgLoss        = avgLoss;
    result.profitFactor   = profitFactor;

    return result;
}

void Portfolio::printReport(const BacktestResult& result) {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════╗\n";
    std::cout << "║        RAPPORT DE BACKTEST           ║\n";
    std::cout << "╠══════════════════════════════════════╣\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "║  Capital initial  : $" << std::setw(14) << result.initialCapital << " ║\n";
    std::cout << "║  Capital final    : $" << std::setw(14) << result.finalCapital   << " ║\n";
    std::cout << "║  ROI              :  " << std::setw(13) << result.roi            << "% ║\n";
    std::cout << "║  Max Drawdown     :  " << std::setw(13) << result.maxDrawdown    << "% ║\n";
    std::cout << "║  Win Rate         :  " << std::setw(13) << result.winRate        << "% ║\n";
    std::cout << "╠══════════════════════════════════════╣\n";
    std::cout << "║  Trades totaux    :  " << std::setw(14) << result.totalTrades    << " ║\n";
    std::cout << "║  Trades gagnants  :  " << std::setw(14) << result.winningTrades  << " ║\n";
    std::cout << "║  Trades perdants  :  " << std::setw(14) << result.losingTrades   << " ║\n";
    std::cout << "╠══════════════════════════════════════╣\n";
    std::cout << "║  P&L net total    : $" << std::setw(14) << result.totalPnl       << " ║\n";
    std::cout << "║  Gain moyen       : $" << std::setw(14) << result.avgWin         << " ║\n";
    std::cout << "║  Perte moyenne    : $" << std::setw(14) << result.avgLoss        << " ║\n";
    std::cout << "║  Profit Factor    :  " << std::setw(14) << result.profitFactor   << " ║\n";
    std::cout << "╚══════════════════════════════════════╝\n";
}
