#include "Backtester.hpp"
#include "Cli.hpp"
#include "DataParser.hpp"
#include "Optimizer.hpp"
#include "Report.hpp"
#include "strategies/MovingAverageCrossover.hpp"
#include "strategies/RSIStrategy.hpp"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

std::size_t toPeriod(double value, const std::string &what) {
    if (value < 1.0 || value != std::floor(value)) {
        throw CliError(what + " must be a positive integer");
    }
    return static_cast<std::size_t>(value);
}

std::unique_ptr<Strategy> makeStrategy(const CliOptions &options) {
    const auto &p = options.params;
    if (options.command == Command::MA) {
        return std::make_unique<MovingAverageCrossover>(toPeriod(p[0], "short period"), toPeriod(p[1], "long period"),
                                                        options.useEma ? MaType::EMA : MaType::SMA);
    }
    return std::make_unique<RSIStrategy>(toPeriod(p[0], "RSI period"), p[1], p[2]);
}

std::string costsDescription(const BacktestConfig &config) {
    std::string text = "Capital $" + Report::formatNumber(config.initialCapital) + " | fee " +
                       Report::formatNumber(config.feeRate * 100.0, 3) + "% | slippage " +
                       Report::formatNumber(config.slippageRate * 100.0, 3) + "% | " +
                       Report::formatNumber(config.periodsPerYear, 0) + " bars/year";
    if (config.stopLossPct > 0.0) {
        text += " | SL " + Report::formatNumber(config.stopLossPct) + "%";
    }
    if (config.takeProfitPct > 0.0) {
        text += " | TP " + Report::formatNumber(config.takeProfitPct) + "%";
    }
    return text;
}

int runBacktest(const std::vector<Candle> &candles, const CliOptions &options) {
    auto strategy = makeStrategy(options);
    const Backtester backtester(options.config);
    const BacktestResult result = backtester.run(candles, *strategy);

    std::cout << "[Backtest] " << strategy->name() << "\n";
    if (!options.quiet) {
        Report::printTrades(std::cout, result.trades);
    }
    Report::printSummary(std::cout, result, strategy->name());

    if (!options.exportDir.empty()) {
        std::filesystem::create_directories(options.exportDir);
        const std::filesystem::path dir(options.exportDir);
        Report::writeEquityCsv((dir / "equity.csv").string(), result.equityCurve);
        Report::writeTradesCsv((dir / "trades.csv").string(), result.trades);
        std::cout << "\nExported equity.csv and trades.csv to " << options.exportDir << "\n";
    }
    return 0;
}

int runOptimization(const std::vector<Candle> &candles, const CliOptions &options) {
    if (!options.exportDir.empty()) {
        throw CliError("--export is only available for a single backtest");
    }

    OptimizerOptions optimizerOptions;
    optimizerOptions.trainRatio = options.trainRatio;
    optimizerOptions.threads = options.threads;
    const Optimizer optimizer(candles, options.config, optimizerOptions);

    // An explicit --sl / --tp fixes that parameter instead of searching it.
    const auto sls = [&](std::vector<double> grid) {
        return options.config.stopLossPct > 0.0 ? std::vector<double>{options.config.stopLossPct} : std::move(grid);
    };
    const auto tps = [&](std::vector<double> grid) {
        return options.config.takeProfitPct > 0.0 ? std::vector<double>{options.config.takeProfitPct} : std::move(grid);
    };

    const std::size_t split = optimizer.splitIndex();
    std::cout << "[Optimizer] Train: " << candles.front().timestamp << " -> " << candles[split - 1].timestamp << " ("
              << split << " bars)";
    if (split < candles.size()) {
        std::cout << " | Test: " << candles[split].timestamp << " -> " << candles.back().timestamp << " ("
                  << candles.size() - split << " bars)";
    }
    std::cout << "\n";

    const auto t0 = std::chrono::steady_clock::now();
    std::vector<OptimResult> results;
    std::string title;

    if (options.command == Command::OPTIMIZE_MA) {
        MaGrid grid;
        grid.shortPeriods = {5, 10, 15, 20, 30};
        grid.longPeriods = {20, 50, 100, 150, 200};
        grid.stopLosses = sls({0.0, 3.0, 5.0, 7.0, 10.0});
        grid.takeProfits = tps({0.0, 10.0, 20.0, 30.0});
        grid.type = options.useEma ? MaType::EMA : MaType::SMA;
        results = optimizer.runMA(grid);
        title = options.useEma ? "EMA Crossover" : "MA Crossover";
    } else {
        RsiGrid grid;
        grid.periods = {7, 14, 21};
        grid.oversolds = {25.0, 30.0, 35.0};
        grid.overboughts = {65.0, 70.0, 75.0};
        grid.stopLosses = sls({0.0, 3.0, 5.0, 7.0});
        grid.takeProfits = tps({0.0, 10.0, 15.0, 20.0});
        results = optimizer.runRSI(grid);
        title = "RSI Strategy";
    }

    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    Report::printOptimization(std::cout, results, title);
    std::cout << "\nExecution time: " << Report::formatNumber(ms, 1) << " ms (" << results.size()
              << " configurations tested)\n";
    return 0;
}

} // namespace

int main(int argc, char *argv[]) {
    const std::string program = argc > 0 ? argv[0] : "backtest";
    const std::vector<std::string> args(argv + (argc > 0 ? 1 : 0), argv + argc);

    CliOptions options;
    try {
        options = parseCli(args);
    } catch (const CliError &e) {
        std::cerr << "[ERROR] " << e.what() << "\n\n" << usage(program);
        return 2;
    }
    if (options.help) {
        std::cout << usage(program);
        return 0;
    }

    try {
        std::vector<std::string> warnings;
        const auto candles = DataParser::loadCSV(options.csvPath, &warnings);
        for (const auto &warning : warnings) {
            std::cerr << "[DataParser] " << warning << "\n";
        }
        if (candles.empty()) {
            std::cerr << "[ERROR] No valid data found in " << options.csvPath << "\n";
            return 1;
        }
        if (!options.periodsPerYearSet) {
            if (const auto inferred = DataParser::inferPeriodsPerYear(candles)) {
                options.config.periodsPerYear = *inferred;
            }
        }
        std::cout << "[DataParser] " << candles.size() << " candles loaded from " << options.csvPath << " ("
                  << candles.front().timestamp << " -> " << candles.back().timestamp << ")\n";
        std::cout << "[Config] " << costsDescription(options.config) << "\n";

        if (options.command == Command::OPTIMIZE_MA || options.command == Command::OPTIMIZE_RSI) {
            return runOptimization(candles, options);
        }
        return runBacktest(candles, options);
    } catch (const CliError &e) {
        std::cerr << "[ERROR] " << e.what() << "\n";
        return 2;
    } catch (const std::exception &e) {
        std::cerr << "[ERROR] " << e.what() << "\n";
        return 1;
    }
}
