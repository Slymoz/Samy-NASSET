#include "Cli.hpp"

#include <gtest/gtest.h>

TEST(Cli, SingleBacktestWithOptionsAnywhere) {
    const auto options = parseCli({"data.csv", "--sl", "5", "ma", "10", "--tp", "20", "50", "--quiet"});
    EXPECT_EQ(options.csvPath, "data.csv");
    EXPECT_EQ(options.command, Command::MA);
    ASSERT_EQ(options.params.size(), 2U);
    EXPECT_DOUBLE_EQ(options.params[0], 10.0);
    EXPECT_DOUBLE_EQ(options.params[1], 50.0);
    EXPECT_DOUBLE_EQ(options.config.stopLossPct, 5.0);
    EXPECT_DOUBLE_EQ(options.config.takeProfitPct, 20.0);
    EXPECT_TRUE(options.quiet);
}

TEST(Cli, DefaultsForMissingStrategyParameters) {
    const auto options = parseCli({"data.csv", "rsi", "21"});
    EXPECT_EQ(options.command, Command::RSI);
    EXPECT_DOUBLE_EQ(options.params[0], 21.0);
    EXPECT_DOUBLE_EQ(options.params[1], 30.0);
    EXPECT_DOUBLE_EQ(options.params[2], 70.0);
}

TEST(Cli, CostsAreGivenInPercent) {
    const auto options = parseCli({"data.csv", "ma", "--fee", "0.1", "--slippage", "0", "--capital", "500"});
    EXPECT_DOUBLE_EQ(options.config.feeRate, 0.001);
    EXPECT_DOUBLE_EQ(options.config.slippageRate, 0.0);
    EXPECT_DOUBLE_EQ(options.config.initialCapital, 500.0);
}

TEST(Cli, Optimize) {
    const auto options = parseCli({"data.csv", "optimize", "rsi", "--train", "0.8", "--threads", "2"});
    EXPECT_EQ(options.command, Command::OPTIMIZE_RSI);
    EXPECT_DOUBLE_EQ(options.trainRatio, 0.8);
    EXPECT_EQ(options.threads, 2U);
    EXPECT_FALSE(options.periodsPerYearSet);
}

TEST(Cli, Help) {
    EXPECT_TRUE(parseCli({"--help"}).help);
    EXPECT_TRUE(parseCli({"data.csv", "ma", "-h"}).help);
}

TEST(Cli, Errors) {
    EXPECT_THROW(parseCli({}), CliError);
    EXPECT_THROW(parseCli({"data.csv"}), CliError);
    EXPECT_THROW(parseCli({"data.csv", "macd"}), CliError);
    EXPECT_THROW(parseCli({"data.csv", "ma", "ten"}), CliError);
    EXPECT_THROW(parseCli({"data.csv", "ma", "10", "50", "99"}), CliError);
    EXPECT_THROW(parseCli({"data.csv", "ma", "--sl"}), CliError);
    EXPECT_THROW(parseCli({"data.csv", "ma", "--sl", "5x"}), CliError);
    EXPECT_THROW(parseCli({"data.csv", "ma", "--sl", "-5"}), CliError);
    EXPECT_THROW(parseCli({"data.csv", "ma", "--unknown"}), CliError);
    EXPECT_THROW(parseCli({"data.csv", "optimize"}), CliError);
    EXPECT_THROW(parseCli({"data.csv", "optimize", "ma", "--train", "0"}), CliError);
    EXPECT_THROW(parseCli({"data.csv", "optimize", "ma", "--threads", "1.5"}), CliError);
}
