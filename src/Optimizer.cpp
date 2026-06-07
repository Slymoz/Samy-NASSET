#include "../include/Optimizer.hpp"
#include "../src/strategies/MovingAverageCrossover.hpp"
#include "../src/strategies/RSIStrategy.hpp"

#include <iostream>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <sstream>

Optimizer::Optimizer(const std::vector<Candle>& candles,
                     double initialCapital,
                     double feeRate)
    : m_candles(candles)
    , m_initialCapital(initialCapital)
    , m_feeRate(feeRate)
{}

double Optimizer::score(const OptimResult& r) {
    // Score composite : ROI * ProfitFactor / (1 + DrawDown)
    // Pénalise les configs avec trop peu de trades (< 3)
    if (r.totalTrades < 3) return -9999.0;
    return (r.roi * r.profitFactor) / (1.0 + r.maxDrawdown);
}

// Helper : applique SL/TP à une stratégie et lance le backtest
static OptimResult runSingle(
    Strategy&                  strategy,
    const std::string&         label,
    double                     sl,
    double                     tp,
    const std::vector<Candle>& candles,
    double                     capital,
    double                     feeRate
) {
    if (sl > 0.0) strategy.setStopLoss(sl);
    if (tp > 0.0) strategy.setTakeProfit(tp);

    Portfolio      portfolio(capital, feeRate);
    BacktestResult result = portfolio.run(candles, strategy, true);

    return OptimResult{
        label,
        result.roi,
        result.maxDrawdown,
        result.winRate,
        result.profitFactor,
        result.totalTrades,
        sl,
        tp
    };
}

std::vector<OptimResult> Optimizer::runMA(
    const std::vector<int>&    shortPeriods,
    const std::vector<int>&    longPeriods,
    const std::vector<double>& stopLosses,
    const std::vector<double>& takeProfits
) {
    std::vector<OptimResult> results;

    int total = 0;
    for (int s : shortPeriods)
        for (int l : longPeriods)
            if (s < l)
                total += static_cast<int>(stopLosses.size() * takeProfits.size());

    std::cout << "\n[Optimizer] MA Crossover — " << total << " configurations à tester...\n\n";

    int count = 0;
    for (int shortP : shortPeriods) {
        for (int longP : longPeriods) {
            if (shortP >= longP) continue; // invalide

            for (double sl : stopLosses) {
                for (double tp : takeProfits) {
                    // Construire le label
                    std::ostringstream oss;
                    oss << "MA(" << shortP << "/" << longP << ")";
                    if (sl > 0.0) oss << " SL=" << sl << "%";
                    if (tp > 0.0) oss << " TP=" << tp << "%";

                    // Instancier la stratégie
                    MovingAverageCrossover strategy(shortP, longP, m_feeRate);

                    auto res = runSingle(
                        strategy, oss.str(), sl, tp,
                        m_candles, m_initialCapital, m_feeRate
                    );

                    results.push_back(res);

                    // Barre de progression
                    ++count;
                    std::cout << "\r  Progression : " << count << "/" << total
                              << " (" << std::fixed << std::setprecision(0)
                              << (100.0 * count / total) << "%)   " << std::flush;
                }
            }
        }
    }

    std::cout << "\n";
    return results;
}

std::vector<OptimResult> Optimizer::runRSI(
    const std::vector<int>&    periods,
    const std::vector<double>& oversolds,
    const std::vector<double>& overboughts,
    const std::vector<double>& stopLosses,
    const std::vector<double>& takeProfits
) {
    std::vector<OptimResult> results;

    int total = static_cast<int>(
        periods.size() * oversolds.size() * overboughts.size()
        * stopLosses.size() * takeProfits.size()
    );

    std::cout << "\n[Optimizer] RSI — " << total << " configurations à tester...\n\n";

    int count = 0;
    for (int period : periods) {
        for (double os : oversolds) {
            for (double ob : overboughts) {
                if (os >= ob) continue;

                for (double sl : stopLosses) {
                    for (double tp : takeProfits) {
                        std::ostringstream oss;
                        oss << "RSI(" << period << " OS=" << os << " OB=" << ob << ")";
                        if (sl > 0.0) oss << " SL=" << sl << "%";
                        if (tp > 0.0) oss << " TP=" << tp << "%";

                        RSIStrategy strategy(period, os, ob, m_feeRate);

                        auto res = runSingle(
                            strategy, oss.str(), sl, tp,
                            m_candles, m_initialCapital, m_feeRate
                        );

                        results.push_back(res);

                        ++count;
                        std::cout << "\r  Progression : " << count << "/" << total
                                  << " (" << std::fixed << std::setprecision(0)
                                  << (100.0 * count / total) << "%)   " << std::flush;
                    }
                }
            }
        }
    }

    std::cout << "\n";
    return results;
}

void Optimizer::printResults(
    const std::vector<OptimResult>& results,
    const std::string&              stratName
) {
    if (results.empty()) {
        std::cout << "[Optimizer] Aucun résultat.\n";
        return;
    }

    // Tri par score décroissant
    auto sorted = results;
    std::sort(sorted.begin(), sorted.end(), [](const OptimResult& a, const OptimResult& b) {
        return Optimizer::score(a) > Optimizer::score(b);
    });

    const auto& best = sorted.front();

    // ✅ hline prend des std::string au lieu de char
    auto hline = [](
        const std::string& left,
        const std::string& fill,
        const std::string& right,
        int                count
    ) {
        std::cout << left;
        for (int i = 0; i < count; ++i) std::cout << fill;
        std::cout << right << "\n";
    };

    // Largeur en nombre de colonnes "fill"
    constexpr int W = 68;

    std::cout << "\n";
    hline("╔", "═", "╗", W);
    std::cout << "║  OPTIMISATION : " << std::left
              << std::setw(W - 17) << stratName << "║\n";
    hline("╠", "═", "╣", W);

    // En-tête colonnes
    std::cout << "║ " << std::left  << std::setw(32) << "Configuration"
              << " │ " << std::right << std::setw(7)  << "ROI"
              << " │ " << std::setw(7)  << "MaxDD"
              << " │ " << std::setw(5)  << "W%"
              << " │ " << std::setw(5)  << "PF"
              << " │ " << std::setw(5)  << "Trades"
              << " ║\n";

    hline("╠", "═", "╣", W);

    int shown = 0;
    for (const auto& r : sorted) {
        if (shown >= 10) break;

        bool isBest = (shown == 0);

        // Tronquer le label si trop long
        std::string label = r.label.size() > 30
                          ? r.label.substr(0, 30)
                          : r.label;

        std::cout << "║ "
                  << (isBest ? "★ " : "  ")
                  << std::left  << std::setw(30) << label
                  << " │ " << std::right << std::fixed << std::setprecision(2)
                  << std::setw(6)  << r.roi           << "%"
                  << " │ " << std::setw(6)  << r.maxDrawdown  << "%"
                  << " │ " << std::setw(4)  << r.winRate      << "%"
                  << " │ " << std::setw(5)  << r.profitFactor
                  << " │ " << std::setw(5)  << r.totalTrades
                  << " ║\n";
        ++shown;
    }

    hline("╠", "═", "╣", W);

    // Résumé meilleure config
    std::string bestLabel = best.label.size() > W - 15
                          ? best.label.substr(0, W - 15)
                          : best.label;

    std::cout << "║  MEILLEUR : " << std::left
              << std::setw(W - 13) << bestLabel << "║\n";

    std::ostringstream summary;
    summary << std::fixed << std::setprecision(2)
            << "  ROI: " << best.roi << "%"
            << "  MaxDD: " << best.maxDrawdown << "%"
            << "  PF: " << best.profitFactor
            << "  Trades: " << best.totalTrades;

    std::cout << "║" << std::left << std::setw(W) << summary.str() << "║\n";

    hline("╚", "═", "╝", W);
}
