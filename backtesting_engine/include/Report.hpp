#pragma once

#include "Optimizer.hpp"
#include "Types.hpp"

#include <cstddef>
#include <ostream>
#include <string>
#include <vector>

// Console output and CSV exports. Kept out of the engine so that the
// simulation code never prints anything.
namespace Report {

std::string formatNumber(double value, int precision = 2);

void printSummary(std::ostream &out, const BacktestResult &result, const std::string &title);

void printTrades(std::ostream &out, const std::vector<TradeRecord> &trades);

void printOptimization(std::ostream &out, const std::vector<OptimResult> &results, const std::string &title,
                       std::size_t top = 10);

void writeEquityCsv(const std::string &path, const std::vector<EquityPoint> &equity);

void writeTradesCsv(const std::string &path, const std::vector<TradeRecord> &trades);

} // namespace Report
