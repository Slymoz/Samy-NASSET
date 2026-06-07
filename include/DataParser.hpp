#pragma once

#include "Types.hpp"
#include <vector>
#include <string>

class DataParser {
public:
    // Charge un fichier CSV et retourne un vecteur de Candles
    // Format attendu : timestamp,open,high,low,close,volume
    static std::vector<Candle> loadCSV(const std::string& filepath);

private:
    // Trim des espaces/retours chariot pour parsing robuste
    static std::string trim(const std::string& str);
};
