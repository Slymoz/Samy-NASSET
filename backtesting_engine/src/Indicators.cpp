#include "Indicators.hpp"

#include <limits>
#include <stdexcept>

namespace {

constexpr double NaN = std::numeric_limits<double>::quiet_NaN();

void requirePositive(std::size_t period, const char *name) {
    if (period == 0) {
        throw std::invalid_argument(std::string("The ") + name + " period must be > 0");
    }
}

double rsiFromAverages(double avgGain, double avgLoss) {
    if (avgLoss == 0.0) {
        return avgGain == 0.0 ? 50.0 : 100.0; // flat market is neutral, not overbought
    }
    const double rs = avgGain / avgLoss;
    return 100.0 - (100.0 / (1.0 + rs));
}

} // namespace

namespace Indicators {

std::vector<double> SMA(const std::vector<double> &prices, std::size_t period) {
    requirePositive(period, "SMA");

    std::vector<double> result(prices.size(), NaN);
    const auto divisor = static_cast<double>(period);
    double sum = 0.0;

    for (std::size_t i = 0; i < prices.size(); ++i) {
        sum += prices[i];
        if (i >= period) {
            sum -= prices[i - period];
        }
        if (i + 1 >= period) {
            result[i] = sum / divisor;
        }
    }
    return result;
}

std::vector<double> EMA(const std::vector<double> &prices, std::size_t period) {
    requirePositive(period, "EMA");

    std::vector<double> result(prices.size(), NaN);
    if (prices.size() < period) {
        return result;
    }

    const auto divisor = static_cast<double>(period);
    const double multiplier = 2.0 / (divisor + 1.0);

    double seed = 0.0;
    for (std::size_t i = 0; i < period; ++i) {
        seed += prices[i];
    }
    result[period - 1] = seed / divisor;

    for (std::size_t i = period; i < prices.size(); ++i) {
        result[i] = (prices[i] - result[i - 1]) * multiplier + result[i - 1];
    }
    return result;
}

std::vector<double> RSI(const std::vector<double> &prices, std::size_t period) {
    requirePositive(period, "RSI");

    std::vector<double> result(prices.size(), NaN);
    if (prices.size() <= period) {
        return result;
    }

    const auto divisor = static_cast<double>(period);
    double avgGain = 0.0;
    double avgLoss = 0.0;

    for (std::size_t i = 1; i <= period; ++i) {
        const double change = prices[i] - prices[i - 1];
        if (change > 0.0) {
            avgGain += change;
        } else {
            avgLoss -= change;
        }
    }
    avgGain /= divisor;
    avgLoss /= divisor;
    result[period] = rsiFromAverages(avgGain, avgLoss);

    for (std::size_t i = period + 1; i < prices.size(); ++i) {
        const double change = prices[i] - prices[i - 1];
        const double gain = change > 0.0 ? change : 0.0;
        const double loss = change < 0.0 ? -change : 0.0;

        avgGain = (avgGain * (divisor - 1.0) + gain) / divisor;
        avgLoss = (avgLoss * (divisor - 1.0) + loss) / divisor;
        result[i] = rsiFromAverages(avgGain, avgLoss);
    }
    return result;
}

} // namespace Indicators
