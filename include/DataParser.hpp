#pragma once

#include <string>
#include <vector>
#include "Types.hpp"

class DataParser {

  public:
    static std::vector<Candle> loadCSV(const std::string &filepath);

  private:
    static std::string trim(const std::string &str);
};
