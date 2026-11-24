#include "DataParser.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace {

std::string trim(const std::string &str) {
    const auto start = str.find_first_not_of(" \t\r\n\"");
    if (start == std::string::npos) {
        return "";
    }
    const auto end = str.find_last_not_of(" \t\r\n\"");
    return str.substr(start, end - start + 1);
}

std::string lower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return str;
}

std::vector<std::string> splitLine(const std::string &line) {
    std::vector<std::string> fields;
    std::stringstream stream(line);
    std::string field;
    while (std::getline(stream, field, ',')) {
        fields.push_back(trim(field));
    }
    if (!line.empty() && line.back() == ',') {
        fields.emplace_back();
    }
    return fields;
}

enum Column : std::size_t { TIMESTAMP, OPEN, HIGH, LOW, CLOSE, VOLUME, COLUMN_COUNT };

std::array<std::size_t, COLUMN_COUNT> locateColumns(const std::vector<std::string> &header) {
    constexpr std::size_t MISSING = static_cast<std::size_t>(-1);
    std::array<std::size_t, COLUMN_COUNT> index{};
    index.fill(MISSING);

    for (std::size_t i = 0; i < header.size(); ++i) {
        const std::string name = lower(header[i]);
        if (index[TIMESTAMP] == MISSING &&
            (name == "timestamp" || name == "date" || name == "datetime" || name == "time")) {
            index[TIMESTAMP] = i;
        } else if (name == "open") {
            index[OPEN] = i;
        } else if (name == "high") {
            index[HIGH] = i;
        } else if (name == "low") {
            index[LOW] = i;
        } else if (name == "close") {
            index[CLOSE] = i;
        } else if (name == "volume") {
            index[VOLUME] = i;
        }
    }

    const std::array<const char *, COLUMN_COUNT> names = {"timestamp", "open", "high", "low", "close", "volume"};
    for (std::size_t c = 0; c < COLUMN_COUNT; ++c) {
        if (index[c] == MISSING) {
            throw std::runtime_error(std::string("CSV header has no '") + names[c] + "' column");
        }
    }
    return index;
}

double parseNumber(const std::string &text) {
    std::size_t consumed = 0;
    const double value = std::stod(text, &consumed);
    if (consumed != text.size()) {
        throw std::invalid_argument("not a number: '" + text + "'");
    }
    if (!std::isfinite(value)) {
        throw std::invalid_argument("non-finite value");
    }
    return value;
}

// Days since 1970-01-01 in the proleptic Gregorian calendar (H. Hinnant).
std::int64_t daysFromCivil(std::int64_t y, unsigned m, unsigned d) {
    y -= m <= 2 ? 1 : 0;
    const std::int64_t era = (y >= 0 ? y : y - 399) / 400;
    const auto yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? static_cast<unsigned>(-3) : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<std::int64_t>(doe) - 719468;
}

void warn(std::vector<std::string> *warnings, const std::string &message) {
    if (warnings != nullptr) {
        warnings->push_back(message);
    }
}

} // namespace

namespace DataParser {

std::optional<std::int64_t> parseTimestamp(const std::string &timestamp) {
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    char sep = 0;

    const int fields =
        std::sscanf(timestamp.c_str(), "%4d-%2d-%2d%c%2d:%2d:%2d", &year, &month, &day, &sep, &hour, &minute, &second);
    // 3 fields: date only; 6: date + HH:MM; 7: date + HH:MM:SS.
    if (fields < 3 || fields == 4 || fields == 5 || (fields > 3 && sep != ' ' && sep != 'T')) {
        return std::nullopt;
    }
    if (month < 1 || month > 12 || day < 1 || day > 31 || hour < 0 || hour > 23 || minute < 0 || minute > 59 ||
        second < 0 || second > 60) {
        return std::nullopt;
    }

    const std::int64_t days = daysFromCivil(year, static_cast<unsigned>(month), static_cast<unsigned>(day));
    return days * 86400 + static_cast<std::int64_t>(hour) * 3600 + static_cast<std::int64_t>(minute) * 60 + second;
}

std::vector<Candle> parseCSV(std::istream &input, std::vector<std::string> *warnings) {
    std::vector<Candle> candles;
    std::string line;
    std::size_t lineNum = 0;
    std::array<std::size_t, COLUMN_COUNT> col{};
    bool haveHeader = false;
    std::size_t needed = 0;

    while (std::getline(input, line)) {
        ++lineNum;
        line = trim(line);
        if (line.empty()) {
            continue;
        }

        const auto fields = splitLine(line);
        if (!haveHeader) {
            col = locateColumns(fields);
            needed = *std::max_element(col.begin(), col.end()) + 1;
            haveHeader = true;
            continue;
        }

        const std::string where = "Line " + std::to_string(lineNum) + " ignored: ";
        if (fields.size() < needed) {
            warn(warnings, where + "missing columns");
            continue;
        }

        Candle candle;
        try {
            candle.timestamp = fields[col[TIMESTAMP]];
            candle.open = parseNumber(fields[col[OPEN]]);
            candle.high = parseNumber(fields[col[HIGH]]);
            candle.low = parseNumber(fields[col[LOW]]);
            candle.close = parseNumber(fields[col[CLOSE]]);
            candle.volume = parseNumber(fields[col[VOLUME]]);
        } catch (const std::exception &e) {
            warn(warnings, where + e.what());
            continue;
        }

        if (candle.open <= 0.0 || candle.high <= 0.0 || candle.low <= 0.0 || candle.close <= 0.0) {
            warn(warnings, where + "non-positive price");
            continue;
        }
        if (candle.high < candle.low || candle.high < std::max(candle.open, candle.close) ||
            candle.low > std::min(candle.open, candle.close)) {
            warn(warnings, where + "inconsistent high/low");
            continue;
        }
        candles.push_back(std::move(candle));
    }

    if (!haveHeader) {
        throw std::runtime_error("The CSV file is empty.");
    }

    // Chronological order, when timestamps can be read.
    std::vector<std::int64_t> times;
    times.reserve(candles.size());
    for (const auto &candle : candles) {
        const auto t = parseTimestamp(candle.timestamp);
        if (!t) {
            return candles; // unknown format: trust the file order
        }
        times.push_back(*t);
    }
    if (!std::is_sorted(times.begin(), times.end())) {
        std::vector<std::size_t> order(candles.size());
        for (std::size_t i = 0; i < order.size(); ++i) {
            order[i] = i;
        }
        std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return times[a] < times[b]; });
        std::vector<Candle> sorted;
        sorted.reserve(candles.size());
        for (std::size_t i : order) {
            sorted.push_back(std::move(candles[i]));
        }
        candles = std::move(sorted);
        warn(warnings, "Candles were not in chronological order: sorted by timestamp.");
    }
    if (std::adjacent_find(times.begin(), times.end()) != times.end()) {
        warn(warnings, "Duplicate timestamps found.");
    }
    return candles;
}

std::vector<Candle> loadCSV(const std::string &filepath, std::vector<std::string> *warnings) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filepath);
    }
    return parseCSV(file, warnings);
}

std::optional<double> inferPeriodsPerYear(const std::vector<Candle> &candles) {
    constexpr std::size_t SAMPLE = 200;
    std::vector<std::int64_t> deltas;
    std::optional<std::int64_t> previous;

    for (std::size_t i = 0; i < candles.size() && i <= SAMPLE; ++i) {
        const auto t = parseTimestamp(candles[i].timestamp);
        if (!t) {
            return std::nullopt;
        }
        if (previous && *t > *previous) {
            deltas.push_back(*t - *previous);
        }
        previous = t;
    }
    if (deltas.empty()) {
        return std::nullopt;
    }

    const auto middle = deltas.begin() + static_cast<std::ptrdiff_t>(deltas.size() / 2);
    std::nth_element(deltas.begin(), middle, deltas.end());
    constexpr double SECONDS_PER_YEAR = 365.0 * 86400.0;
    return SECONDS_PER_YEAR / static_cast<double>(*middle);
}

} // namespace DataParser
