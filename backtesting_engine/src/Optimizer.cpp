#include "../include/Optimizer.hpp"
#include "../src/strategies/MovingAverageCrossover.hpp"
#include "../src/strategies/RSIStrategy.hpp"

#include <iostream>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <sstream>

Optimizer::Optimizer(const std::vector<Candle> &candles, double initialCapital, double feeRate)
    : m_candles(candles),
      m_initialCapital(initialCapital),
      m_feeRate(feeRate) {}

double Optimizer::score(const OptimResult &r) {
    if (r.totalTrades < 3) {
        return -9999.0;
    }
    return (r.roi * r.profitFactor) / (1.0 + r.maxDrawdown);
}

static OptimResult runSingle(Strategy &strategy, const std::string &label, double sl, double tp,
                             const std::vector<Candle> &candles, double capital, double feeRate) {
    if (sl > 0.0) {
        strategy.setStopLoss(sl);
    }
    if (tp > 0.0) {
        strategy.setTakeProfit(tp);
    }

    Portfolio portfolio(capital, feeRate);
    BacktestResult result = portfolio.run(candles, strategy, true);

    return OptimResult{label, result.roi, result.maxDrawdown, result.winRate, result.profitFactor, result.totalTrades,
                       sl,    tp};
}

std::vector<OptimResult> Optimizer::runMA(const std::vector<int> &shortPeriods, const std::vector<int> &longPeriods,
                                          const std::vector<double> &stopLosses,
                                          const std::vector<double> &takeProfits) {
    std::vector<OptimResult> results;

    int total = 0;
    for (int s : shortPeriods) {
        for (int l : longPeriods) {
            if (s < l) {
                total += static_cast<int>(stopLosses.size() * takeProfits.size());
            }
        }
    }

    std::cout << "\n[Optimizer] MA Crossover — " << total << " configurations to test...\n\n";

    int count = 0;
    for (int shortP : shortPeriods) {
        for (int longP : longPeriods) {
            if (shortP >= longP) {
                continue;
            }

            for (double sl : stopLosses) {
                for (double tp : takeProfits) {
                    std::ostringstream oss;
                    oss << "MA(" << shortP << "/" << longP << ")";
                    if (sl > 0.0) {
                        oss << " SL=" << sl << "%";
                    }
                    if (tp > 0.0) {
                        oss << " TP=" << tp << "%";
                    }

                    MovingAverageCrossover strategy(shortP, longP, m_feeRate);

                    auto res = runSingle(strategy, oss.str(), sl, tp, m_candles, m_initialCapital, m_feeRate);

                    results.push_back(res);

                    ++count;
                    std::cout << "\r  Progression : " << count << "/" << total << " (" << std::fixed
                              << std::setprecision(0) << (100.0 * count / total) << "%)   " << std::flush;
                }
            }
        }
    }

    std::cout << "\n";
    return results;
}

std::vector<OptimResult> Optimizer::runRSI(const std::vector<int> &periods, const std::vector<double> &oversolds,
                                           const std::vector<double> &overboughts,
                                           const std::vector<double> &stopLosses,
                                           const std::vector<double> &takeProfits) {
    std::vector<OptimResult> results;

    int total = static_cast<int>(periods.size() * oversolds.size() * overboughts.size() * stopLosses.size() *
                                 takeProfits.size());

    std::cout << "\n[Optimizer] RSI — " << total << " configurations to test...\n\n";

    int count = 0;
    for (int period : periods) {
        for (double os : oversolds) {
            for (double ob : overboughts) {
                if (os >= ob) {
                    continue;
                }

                for (double sl : stopLosses) {
                    for (double tp : takeProfits) {
                        std::ostringstream oss;
                        oss << "RSI(" << period << " OS=" << os << " OB=" << ob << ")";
                        if (sl > 0.0) {
                            oss << " SL=" << sl << "%";
                        }
                        if (tp > 0.0) {
                            oss << " TP=" << tp << "%";
                        }

                        RSIStrategy strategy(period, os, ob, m_feeRate);

                        auto res = runSingle(strategy, oss.str(), sl, tp, m_candles, m_initialCapital, m_feeRate);

                        results.push_back(res);

                        ++count;
                        std::cout << "\r  Progression : " << count << "/" << total << " (" << std::fixed
                                  << std::setprecision(0) << (100.0 * count / total) << "%)   " << std::flush;
                    }
                }
            }
        }
    }

    std::cout << "\n";
    return results;
}

void Optimizer::printResults(const std::vector<OptimResult> &results, const std::string &stratName) {
    if (results.empty()) {
        std::cout << "[Optimizer] No results found.\n";
        return;
    }

    auto sorted = results;
    std::sort(sorted.begin(), sorted.end(),
              [](const OptimResult &a, const OptimResult &b) { return Optimizer::score(a) > Optimizer::score(b); });

    const auto &best = sorted.front();

    const int cConfig = 34;
    const int cROI = 9;
    const int cMaxDD = 9;
    const int cWR = 8;
    const int cPF = 8;
    const int cTrades = 8;

    auto sep = [&]() {
        std::cout << "+";
        for (int i = 0; i < cConfig + 2; ++i) {
            std::cout << "-";
        }
        std::cout << "+";
        for (int i = 0; i < cROI + 2; ++i) {
            std::cout << "-";
        }
        std::cout << "+";
        for (int i = 0; i < cMaxDD + 2; ++i) {
            std::cout << "-";
        }
        std::cout << "+";
        for (int i = 0; i < cWR + 2; ++i) {
            std::cout << "-";
        }
        std::cout << "+";
        for (int i = 0; i < cPF + 2; ++i) {
            std::cout << "-";
        }
        std::cout << "+";
        for (int i = 0; i < cTrades + 2; ++i) {
            std::cout << "-";
        }
        std::cout << "+\n";
    };

    const int totalW = cConfig + cROI + cMaxDD + cWR + cPF + cTrades + 19;

    std::cout << "\n";

    std::cout << "+";
    for (int i = 0; i < totalW - 2; ++i) {
        std::cout << "-";
    }
    std::cout << "+\n";

    std::string title = " OPTIMISATION : " + stratName;
    std::cout << "|" << std::left << std::setw(totalW - 2) << title << "|\n";

    sep();

    std::cout << "| " << std::left << std::setw(cConfig) << "Configuration" << " | " << std::right << std::setw(cROI)
              << "ROI" << " | " << std::setw(cMaxDD) << "MaxDD" << " | " << std::setw(cWR) << "W%" << " | "
              << std::setw(cPF) << "PF" << " | " << std::setw(cTrades) << "Trades" << " |\n";

    sep();

    int shown = 0;
    for (const auto &r : sorted) {
        if (shown >= 10) {
            break;
        }

        std::string marker = (shown == 0) ? "* " : "  ";
        std::string label = r.label;
        if (label.size() > static_cast<size_t>(cConfig - 2)) {
            label = label.substr(0, cConfig - 2);
        }

        std::ostringstream sROI, sMaxDD, sWR, sPF;
        sROI << std::fixed << std::setprecision(2) << r.roi << "%";
        sMaxDD << std::fixed << std::setprecision(2) << r.maxDrawdown << "%";
        sWR << std::fixed << std::setprecision(1) << r.winRate << "%";
        sPF << std::fixed << std::setprecision(2) << r.profitFactor;

        std::cout << "| " << marker << std::left << std::setw(cConfig - 2) << label << " | " << std::right
                  << std::setw(cROI) << sROI.str() << " | " << std::setw(cMaxDD) << sMaxDD.str() << " | "
                  << std::setw(cWR) << sWR.str() << " | " << std::setw(cPF) << sPF.str() << " | " << std::setw(cTrades)
                  << r.totalTrades << " |\n";
        ++shown;
    }

    sep();

    std::string bestLabel = " BEST : " + best.label;
    if (bestLabel.size() > static_cast<size_t>(totalW - 2)) {
        bestLabel = bestLabel.substr(0, totalW - 2);
    }

    std::cout << "|" << std::left << std::setw(totalW - 2) << bestLabel << "|\n";

    std::ostringstream summary;
    summary << " ROI: " << std::fixed << std::setprecision(2) << best.roi << "%" << "  |  MaxDD: " << best.maxDrawdown
            << "%" << "  |  PF: " << best.profitFactor << "  |  Trades: " << best.totalTrades;

    std::cout << "|" << std::left << std::setw(totalW - 2) << summary.str() << "|\n";

    std::cout << "+";
    for (int i = 0; i < totalW - 2; ++i) {
        std::cout << "-";
    }
    std::cout << "+\n";
}
