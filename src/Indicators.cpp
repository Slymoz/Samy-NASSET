#include "../include/Indicators.hpp"
#include <stdexcept>
#include <numeric>

std::vector<double> Indicators::SMA(const std::vector<double> &prices, int period) {
    if (period <= 0) {
        throw std::invalid_argument("The SMA period must be > 0");
    }

    std::vector<double> result(prices.size(), 0.0);
    double sum = 0.0;

    for (size_t i = 0; i < prices.size(); ++i) {
        sum += prices[i];
        if (static_cast<int>(i) >= period) {
            sum -= prices[i - period];
        }
        if (static_cast<int>(i) >= period - 1) {
            result[i] = sum / period;
        }
    }
    return result;
}

std::vector<double> Indicators::EMA(const std::vector<double> &prices, int period) {
    if (period <= 0) {
        throw std::invalid_argument("The EMA period must be > 0");
    }

    std::vector<double> result(prices.size(), 0.0);
    double multiplier = 2.0 / (period + 1);

    double initialSMA = 0.0;
    for (int i = 0; i < period && i < static_cast<int>(prices.size()); ++i) {
        initialSMA += prices[i];
    }
    initialSMA /= period;

    result[period - 1] = initialSMA;

    for (size_t i = period; i < prices.size(); ++i) {
        result[i] = (prices[i] - result[i - 1]) * multiplier + result[i - 1];
    }
    return result;
}

std::vector<double> Indicators::RSI(const std::vector<double> &prices, int period) {
    if (period <= 0) {
        throw std::invalid_argument("The RSI period must be > 0");
    }

    std::vector<double> result(prices.size(), 0.0);

    double avgGain = 0.0;
    double avgLoss = 0.0;

    for (int i = 1; i <= period && i < static_cast<int>(prices.size()); ++i) {
        double change = prices[i] - prices[i - 1];
        if (change > 0) {
            avgGain += change;
        } else {
            avgLoss += (-change);
        }
    }
    avgGain /= period;
    avgLoss /= period;

    auto calcRSI = [](double gain, double loss) -> double {
        if (loss == 0.0) {
            return 100.0;
        }
        double rs = gain / loss;
        return 100.0 - (100.0 / (1.0 + rs));
    };

    result[period] = calcRSI(avgGain, avgLoss);

    for (size_t i = period + 1; i < prices.size(); ++i) {
        double change = prices[i] - prices[i - 1];
        double gain = (change > 0) ? change : 0.0;
        double loss = (change < 0) ? (-change) : 0.0;

        avgGain = (avgGain * (period - 1) + gain) / period;
        avgLoss = (avgLoss * (period - 1) + loss) / period;

        result[i] = calcRSI(avgGain, avgLoss);
    }
    return result;
}
