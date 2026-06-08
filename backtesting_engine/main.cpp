#include <iostream>
#include <string>
#include <memory>
#include <stdexcept>
#include <chrono>
#include <iomanip>

#include "include/DataParser.hpp"
#include "include/Portfolio.hpp"
#include "include/Optimizer.hpp"
#include "src/strategies/MovingAverageCrossover.hpp"
#include "src/strategies/RSIStrategy.hpp"

void printUsage(const char *progName) {
    std::cout << "Usage: " << progName << " <csv> <strategie> [options]\n\n"
              << "Strategies:\n"
              << "  ma  <short> <long>               Moving Average Crossover\n"
              << "  rsi <period> <oversold> <overbought>  RSI\n\n"
              << "Optimization Mode:\n"
              << "  optimize ma                      Tests all MA configurations\n"
              << "  optimize rsi                     Tests all RSI configurations\n\n"
              << "Options:\n"
              << "  --sl <percent>   Stop-loss\n"
              << "  --tp <percent>   Take-profit\n\n"
              << "Examples:\n"
              << "  " << progName << " data/btc.csv ma 10 50 --sl 5\n"
              << "  " << progName << " data/btc.csv optimize ma\n"
              << "  " << progName << " data/btc.csv optimize rsi\n";
}

double parseFlag(int argc, char *argv[], const std::string &flag, double def) {
    for (int i = 1; i < argc - 1; ++i) {
        if (std::string(argv[i]) == flag) {
            return std::stod(argv[i + 1]);
        }
    }
    return def;
}

bool hasFlag(int argc, char *argv[], const std::string &flag) {
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == flag) {
            return true;
        }
    }
    return false;
}

int main(int argc, char *argv[]) {
    std::cout << "+--------------------------------------+\n"
              << "|   C++ Financial Backtesting Engine   |\n"
              << "+--------------------------------------+\n\n";

    if (argc < 3) {
        printUsage(argv[0]);
        return 1;
    }

    const std::string csvFile = argv[1];
    const std::string stratName = argv[2];
    const double initialCapital = 10000.0;
    const double feeRate = 0.001;

    try {
        auto candles = DataParser::loadCSV(csvFile);
        if (candles.empty()) {
            std::cerr << "[ERROR] No valid data found.\n";
            return 1;
        }

        if (stratName == "optimize") {
            if (argc < 4) {
                printUsage(argv[0]);
                return 1;
            }

            const std::string target = argv[3];
            Optimizer optimizer(candles, initialCapital, feeRate);

            auto t0 = std::chrono::high_resolution_clock::now();

            std::vector<OptimResult> results;

            if (target == "ma") {
                results = optimizer.runMA({5, 10, 15, 20, 30}, {20, 50, 100, 150, 200}, {0.0, 3.0, 5.0, 7.0, 10.0},
                                          {0.0, 10.0, 20.0, 30.0});
                Optimizer::printResults(results, "MA Crossover");
            } else if (target == "rsi") {
                results = optimizer.runRSI({7, 14, 21}, {25.0, 30.0, 35.0}, {65.0, 70.0, 75.0}, {0.0, 3.0, 5.0, 7.0},
                                           {0.0, 10.0, 15.0, 20.0});
                Optimizer::printResults(results, "RSI Strategy");
            } else {
                std::cerr << "[ERROR] Unknown target : " << target << "\n";
                return 1;
            }

            auto t1 = std::chrono::high_resolution_clock::now();
            double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

            std::cout << "\n  Execution time : " << std::fixed << std::setprecision(1) << ms << " ms" << "  ("
                      << results.size() << " configurations tested)\n\n";

            return 0;
        }

        std::unique_ptr<Strategy> strategy;

        if (stratName == "ma") {
            int shortP = (argc > 3) ? std::stoi(argv[3]) : 10;
            int longP = (argc > 4) ? std::stoi(argv[4]) : 50;
            strategy = std::make_unique<MovingAverageCrossover>(shortP, longP, feeRate);
        } else if (stratName == "rsi") {
            int period = (argc > 3) ? std::stoi(argv[3]) : 14;
            double oversold = (argc > 4) ? std::stod(argv[4]) : 30.0;
            double overbought = (argc > 5) ? std::stod(argv[5]) : 70.0;
            strategy = std::make_unique<RSIStrategy>(period, oversold, overbought, feeRate);
        } else {
            std::cerr << "[ERROR] Unknown strategy : " << stratName << "\n";
            printUsage(argv[0]);
            return 1;
        }

        if (hasFlag(argc, argv, "--sl")) {
            double sl = parseFlag(argc, argv, "--sl", 5.0);
            strategy->setStopLoss(sl);
            std::cout << "[Risk] Stop-Loss activated at -" << sl << "%\n";
        }
        if (hasFlag(argc, argv, "--tp")) {
            double tp = parseFlag(argc, argv, "--tp", 15.0);
            strategy->setTakeProfit(tp);
            std::cout << "[Risk] Take-Profit activated at +" << tp << "%\n";
        }

        Portfolio portfolio(initialCapital, feeRate);
        BacktestResult result = portfolio.run(candles, *strategy);
        Portfolio::printReport(result);
    } catch (const std::exception &e) {
        std::cerr << "[EXCEPTION] " << e.what() << "\n";
        return 1;
    }

    return 0;
}
