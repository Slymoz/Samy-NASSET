#pragma once

#include "Types.hpp"

#include <cstdint>
#include <istream>
#include <optional>
#include <string>
#include <vector>

// OHLCV CSV loader.
//
// Columns are located by their header name (case-insensitive, any order,
// extra columns ignored): timestamp|date|datetime|time, open, high, low,
// close, volume. Invalid rows (unparsable numbers, non-positive prices,
// inconsistent high/low) are skipped and reported in `warnings`. When the
// timestamps can be parsed, the candles are returned in chronological order.
namespace DataParser {

std::vector<Candle> loadCSV(const std::string &filepath, std::vector<std::string> *warnings = nullptr);

std::vector<Candle> parseCSV(std::istream &input, std::vector<std::string> *warnings = nullptr);

// Seconds since the Unix epoch for "YYYY-MM-DD" or "YYYY-MM-DD[ T]HH:MM[:SS]"
// (anything after the seconds, such as a timezone, is ignored).
std::optional<std::int64_t> parseTimestamp(const std::string &timestamp);

// Bars per year deduced from the median spacing of the timestamps
// (365 for daily crypto data, 8760 for hourly...). Empty if unknown.
std::optional<double> inferPeriodsPerYear(const std::vector<Candle> &candles);

} // namespace DataParser
