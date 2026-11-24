#pragma once

#include "Types.hpp"

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

// Invalid command line (bad value, unknown option, missing argument).
class CliError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

enum class Command { MA, RSI, OPTIMIZE_MA, OPTIMIZE_RSI };

struct CliOptions {
    bool help = false;
    std::string csvPath;
    Command command = Command::MA;
    std::vector<double> params; // strategy parameters, in order

    BacktestConfig config;
    bool periodsPerYearSet = false; // false = inferred from the timestamps
    bool useEma = false;
    double trainRatio = 0.7;
    unsigned threads = 0;
    std::string exportDir;
    bool quiet = false;
};

// Options may appear anywhere after the program name. Percentages are given
// in percent on the command line (--fee 0.1 means 0.1 %).
CliOptions parseCli(const std::vector<std::string> &args);

std::string usage(const std::string &program);
