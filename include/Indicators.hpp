#pragma once

#include <vector>

// Fonctions statiques de calcul d'indicateurs techniques
class Indicators {
public:
    // Simple Moving Average sur 'period' dernières valeurs
    // Retourne un vecteur de même taille (NaN=0 pour les premières valeurs)
    static std::vector<double> SMA(
        const std::vector<double>& prices,
        int period
    );

    // Exponential Moving Average
    static std::vector<double> EMA(
        const std::vector<double>& prices,
        int period
    );

    // Relative Strength Index
    static std::vector<double> RSI(
        const std::vector<double>& prices,
        int period = 14
    );
};
