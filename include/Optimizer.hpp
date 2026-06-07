#pragma once

#include "Types.hpp"
#include "Portfolio.hpp"
#include <vector>
#include <string>
#include <functional>
#include <memory>

// Résultat d'une configuration testée
struct OptimResult {
    std::string label;
    double      roi;
    double      maxDrawdown;
    double      winRate;
    double      profitFactor;
    int         totalTrades;
    double      stopLoss;    // 0 = désactivé
    double      takeProfit;  // 0 = désactivé
};

class Optimizer {
public:
    explicit Optimizer(
        const std::vector<Candle>& candles,
        double                     initialCapital = 10000.0,
        double                     feeRate        = 0.001
    );

    // Lance l'optimisation MA Crossover
    std::vector<OptimResult> runMA(
        const std::vector<int>&    shortPeriods,
        const std::vector<int>&    longPeriods,
        const std::vector<double>& stopLosses,   // ex: {0, 3, 5, 7} (0 = sans SL)
        const std::vector<double>& takeProfits   // ex: {0, 10, 20}  (0 = sans TP)
    );

    // Lance l'optimisation RSI
    std::vector<OptimResult> runRSI(
        const std::vector<int>&    periods,
        const std::vector<double>& oversolds,
        const std::vector<double>& overboughts,
        const std::vector<double>& stopLosses,
        const std::vector<double>& takeProfits
    );

    // Affiche un tableau formaté + surligne le meilleur
    static void printResults(
        const std::vector<OptimResult>& results,
        const std::string&              stratName
    );

private:
    const std::vector<Candle>& m_candles;
    double                     m_initialCapital;
    double                     m_feeRate;

    // Critère de classement : profit factor pondéré par le ROI
    static double score(const OptimResult& r);
};
