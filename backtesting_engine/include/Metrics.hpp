#pragma once

#include "Types.hpp"

#include <vector>

// Performance statistics. Pure functions on an equity curve or a trade list,
// so they can be tested in isolation.
namespace Metrics {

// Largest peak-to-trough decline, in %.
double maxDrawdown(const std::vector<double> &equity);

// Per-bar simple returns (size = equity.size() - 1).
std::vector<double> returns(const std::vector<double> &equity);

// Annualised Sharpe ratio (risk-free rate = 0, sample standard deviation).
double sharpe(const std::vector<double> &barReturns, double periodsPerYear);

// Annualised Sortino ratio (risk-free rate = 0). +inf when the mean return
// is positive and no bar lost money.
double sortino(const std::vector<double> &barReturns, double periodsPerYear);

// Compound annual growth rate in %, for `bars` bars of data.
double cagr(double initial, double final, double bars, double periodsPerYear);

// Gross profit / gross loss. +inf when there are wins and no losses, 0 when
// there is nothing to measure.
double profitFactor(const std::vector<TradeRecord> &trades);

// Fill the trade statistics of `result` (counts, win rate, averages, PF).
void fillTradeStats(BacktestResult &result);

} // namespace Metrics
