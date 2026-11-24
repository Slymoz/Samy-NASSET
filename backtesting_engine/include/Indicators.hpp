#pragma once

#include <cstddef>
#include <vector>

// Technical indicators. Every function returns a vector of the same size as
// its input. Values that cannot be computed yet (warm-up period, or input
// shorter than the period) are NaN, never 0, so they cannot be mistaken for
// real values.
namespace Indicators {

std::vector<double> SMA(const std::vector<double> &prices, std::size_t period);

// Seeded with the SMA of the first `period` prices.
std::vector<double> EMA(const std::vector<double> &prices, std::size_t period);

// Wilder's RSI. The first value is available at index `period`.
std::vector<double> RSI(const std::vector<double> &prices, std::size_t period = 14);

} // namespace Indicators
