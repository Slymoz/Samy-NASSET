#include "DataParser.hpp"

#include <gtest/gtest.h>

#include <sstream>
#include <stdexcept>

namespace {

std::vector<Candle> parse(const std::string &csv, std::vector<std::string> *warnings = nullptr) {
    std::istringstream input(csv);
    return DataParser::parseCSV(input, warnings);
}

} // namespace

TEST(DataParser, ParsesStandardFile) {
    const auto candles = parse("timestamp,open,high,low,close,volume\n"
                               "2024-01-01,10,12,9,11,100\n"
                               "2024-01-02,11,13,10,12,200\n");
    ASSERT_EQ(candles.size(), 2U);
    EXPECT_EQ(candles[0].timestamp, "2024-01-01");
    EXPECT_DOUBLE_EQ(candles[1].open, 11.0);
    EXPECT_DOUBLE_EQ(candles[1].high, 13.0);
    EXPECT_DOUBLE_EQ(candles[1].low, 10.0);
    EXPECT_DOUBLE_EQ(candles[1].close, 12.0);
    EXPECT_DOUBLE_EQ(candles[1].volume, 200.0);
}

TEST(DataParser, ColumnsAreFoundByNameInAnyOrder) {
    const auto candles = parse("Close,Volume,Date,Low,High,Open,Adj Close\r\n"
                               "11,100,2024-01-01,9,12,10,11\r\n");
    ASSERT_EQ(candles.size(), 1U);
    EXPECT_EQ(candles[0].timestamp, "2024-01-01");
    EXPECT_DOUBLE_EQ(candles[0].open, 10.0);
    EXPECT_DOUBLE_EQ(candles[0].close, 11.0);
}

TEST(DataParser, MissingColumnThrows) {
    EXPECT_THROW(parse("timestamp,open,high,low,close\n2024-01-01,1,1,1,1\n"), std::runtime_error);
    EXPECT_THROW(parse(""), std::runtime_error);
}

TEST(DataParser, InvalidRowsAreSkippedWithAWarning) {
    std::vector<std::string> warnings;
    const auto candles = parse("timestamp,open,high,low,close,volume\n"
                               "2024-01-01,10,12,9,11,100\n"
                               "2024-01-02,abc,12,9,11,100\n" // not a number
                               "2024-01-03,10,8,9,11,100\n"   // high < low
                               "2024-01-04,10,12,9,-1,100\n"  // negative price
                               "2024-01-05,10,12\n"           // missing columns
                               "2024-01-06,10,12,9,11,100\n",
                               &warnings);
    EXPECT_EQ(candles.size(), 2U);
    EXPECT_EQ(warnings.size(), 4U);
}

TEST(DataParser, DescendingFileIsSortedChronologically) {
    std::vector<std::string> warnings;
    const auto candles = parse("timestamp,open,high,low,close,volume\n"
                               "2024-01-03,3,3,3,3,1\n"
                               "2024-01-01,1,1,1,1,1\n"
                               "2024-01-02,2,2,2,2,1\n",
                               &warnings);
    ASSERT_EQ(candles.size(), 3U);
    EXPECT_EQ(candles[0].timestamp, "2024-01-01");
    EXPECT_EQ(candles[2].timestamp, "2024-01-03");
    EXPECT_EQ(warnings.size(), 1U);
}

TEST(DataParser, ParseTimestamp) {
    EXPECT_EQ(DataParser::parseTimestamp("1970-01-01"), 0);
    EXPECT_EQ(DataParser::parseTimestamp("1970-01-02"), 86400);
    EXPECT_EQ(DataParser::parseTimestamp("2024-03-01"), 1709251200);
    EXPECT_EQ(DataParser::parseTimestamp("2024-03-01 01:02:03+00:00"), 1709251200 + 3723);
    EXPECT_EQ(DataParser::parseTimestamp("2024-03-01T01:02"), 1709251200 + 3720);
    EXPECT_FALSE(DataParser::parseTimestamp("yesterday"));
    EXPECT_FALSE(DataParser::parseTimestamp("2024-13-01"));
}

TEST(DataParser, InferPeriodsPerYear) {
    const std::vector<Candle> daily = {
        {"2024-01-01", 1, 1, 1, 1, 1}, {"2024-01-02", 1, 1, 1, 1, 1}, {"2024-01-03", 1, 1, 1, 1, 1}};
    EXPECT_EQ(DataParser::inferPeriodsPerYear(daily), 365.0);

    const std::vector<Candle> hourly = {{"2024-01-01 00:00:00", 1, 1, 1, 1, 1},
                                        {"2024-01-01 01:00:00", 1, 1, 1, 1, 1},
                                        {"2024-01-01 02:00:00", 1, 1, 1, 1, 1}};
    EXPECT_EQ(DataParser::inferPeriodsPerYear(hourly), 8760.0);

    EXPECT_FALSE(DataParser::inferPeriodsPerYear({{"bar 1", 1, 1, 1, 1, 1}, {"bar 2", 1, 1, 1, 1, 1}}));
}
