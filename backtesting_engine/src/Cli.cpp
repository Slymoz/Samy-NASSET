#include "Cli.hpp"

#include <cctype>
#include <cmath>
#include <sstream>

namespace {

double toNumber(const std::string &text, const std::string &what) {
    std::size_t consumed = 0;
    double value = 0.0;
    try {
        value = std::stod(text, &consumed);
    } catch (const std::exception &) {
        consumed = 0;
    }
    if (consumed == 0 || consumed != text.size() || !std::isfinite(value)) {
        throw CliError("Invalid number for " + what + ": '" + text + "'");
    }
    return value;
}

double toNonNegative(const std::string &text, const std::string &what) {
    const double value = toNumber(text, what);
    if (value < 0.0) {
        throw CliError(what + " must be >= 0");
    }
    return value;
}

} // namespace

std::string usage(const std::string &program) {
    std::ostringstream out;
    out << "Usage:\n"
        << "  " << program << " <csv> ma  <short> <long>                  [options]\n"
        << "  " << program << " <csv> rsi <period> <oversold> <overbought> [options]\n"
        << "  " << program << " <csv> optimize <ma|rsi>                   [options]\n\n"
        << "Risk and costs:\n"
        << "  --sl <pct>             Stop-loss, e.g. 5 for -5 % (default: off)\n"
        << "  --tp <pct>             Take-profit, e.g. 20 for +20 % (default: off)\n"
        << "  --capital <amount>     Initial capital (default: 10000)\n"
        << "  --fee <pct>            Fee per side (default: 0.1)\n"
        << "  --slippage <pct>       Adverse price move per fill (default: 0.05)\n\n"
        << "Strategy:\n"
        << "  --ema                  MA crossover on EMAs instead of SMAs\n\n"
        << "Optimization:\n"
        << "  --train <ratio>        Share of the data used to pick parameters; the\n"
        << "                         rest is the out-of-sample test (default: 0.7,\n"
        << "                         1 = no test period)\n"
        << "  --threads <n>          Worker threads (default: all cores)\n\n"
        << "Output:\n"
        << "  --periods-per-year <n> Bars per year for CAGR / Sharpe (default: inferred\n"
        << "                         from the timestamps, e.g. 365 for daily crypto)\n"
        << "  --export <dir>         Write equity.csv and trades.csv to <dir>\n"
        << "  --quiet                Do not print the trade list\n"
        << "  -h, --help             Show this help\n\n"
        << "Examples:\n"
        << "  " << program << " data/btc.csv ma 10 50 --sl 5\n"
        << "  " << program << " data/btc.csv rsi 14 30 70 --tp 15 --export results\n"
        << "  " << program << " data/btc.csv optimize ma --train 0.7\n";
    return out.str();
}

CliOptions parseCli(const std::vector<std::string> &args) {
    CliOptions options;
    std::vector<std::string> positional;

    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string &arg = args[i];

        auto value = [&]() -> const std::string & {
            if (i + 1 >= args.size()) {
                throw CliError("Missing value after " + arg);
            }
            return args[++i];
        };

        if (arg == "-h" || arg == "--help") {
            options.help = true;
            return options;
        }
        if (arg == "--sl") {
            options.config.stopLossPct = toNonNegative(value(), "--sl");
            if (options.config.stopLossPct >= 100.0) {
                throw CliError("--sl must be < 100");
            }
        } else if (arg == "--tp") {
            options.config.takeProfitPct = toNonNegative(value(), "--tp");
        } else if (arg == "--capital") {
            options.config.initialCapital = toNumber(value(), "--capital");
            if (options.config.initialCapital <= 0.0) {
                throw CliError("--capital must be > 0");
            }
        } else if (arg == "--fee") {
            options.config.feeRate = toNonNegative(value(), "--fee") / 100.0;
        } else if (arg == "--slippage") {
            options.config.slippageRate = toNonNegative(value(), "--slippage") / 100.0;
        } else if (arg == "--periods-per-year") {
            options.config.periodsPerYear = toNumber(value(), "--periods-per-year");
            if (options.config.periodsPerYear <= 0.0) {
                throw CliError("--periods-per-year must be > 0");
            }
            options.periodsPerYearSet = true;
        } else if (arg == "--train") {
            options.trainRatio = toNumber(value(), "--train");
            if (!(options.trainRatio > 0.0 && options.trainRatio <= 1.0)) {
                throw CliError("--train must be in (0, 1]");
            }
        } else if (arg == "--threads") {
            const double threads = toNonNegative(value(), "--threads");
            if (threads != std::floor(threads) || threads > 1024.0) {
                throw CliError("--threads must be an integer between 0 and 1024");
            }
            options.threads = static_cast<unsigned>(threads);
        } else if (arg == "--export") {
            options.exportDir = value();
        } else if (arg == "--ema") {
            options.useEma = true;
        } else if (arg == "--quiet") {
            options.quiet = true;
        } else if (arg.size() > 1 && arg[0] == '-' && !std::isdigit(static_cast<unsigned char>(arg[1]))) {
            throw CliError("Unknown option: " + arg);
        } else {
            positional.push_back(arg);
        }
    }

    if (positional.size() < 2) {
        throw CliError("Expected a CSV file and a strategy.");
    }
    options.csvPath = positional[0];
    const std::string &strategy = positional[1];

    std::size_t expected = 0;
    if (strategy == "optimize") {
        if (positional.size() < 3) {
            throw CliError("optimize needs a target: ma or rsi");
        }
        if (positional[2] == "ma") {
            options.command = Command::OPTIMIZE_MA;
        } else if (positional[2] == "rsi") {
            options.command = Command::OPTIMIZE_RSI;
        } else {
            throw CliError("Unknown optimization target: " + positional[2]);
        }
        expected = 3;
    } else if (strategy == "ma") {
        options.command = Command::MA;
        options.params = {10, 50};
        expected = 4;
    } else if (strategy == "rsi") {
        options.command = Command::RSI;
        options.params = {14, 30, 70};
        expected = 5;
    } else {
        throw CliError("Unknown strategy: " + strategy);
    }

    if (positional.size() > expected) {
        throw CliError("Too many arguments: '" + positional[expected] + "'");
    }
    // Strategy parameters are optional; missing ones keep their defaults.
    if (strategy != "optimize") {
        for (std::size_t p = 2; p < positional.size(); ++p) {
            options.params[p - 2] = toNumber(positional[p], "strategy parameter " + std::to_string(p - 1));
        }
    }
    return options;
}
