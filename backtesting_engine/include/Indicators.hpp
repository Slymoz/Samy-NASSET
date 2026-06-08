#pragma once

#include <vector>

class Indicators {

  public:
    static std::vector<double> SMA(const std::vector<double> &prices, int period);
    static std::vector<double> EMA(const std::vector<double> &prices, int period);
    static std::vector<double> RSI(const std::vector<double> &prices, int period = 14);
};
