#include "../include/DataParser.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <iostream>
#include <algorithm>

std::string DataParser::trim(const std::string &str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    size_t end = str.find_last_not_of(" \t\r\n");
    return (start == std::string::npos) ? "" : str.substr(start, end - start + 1);
}

std::vector<Candle> DataParser::loadCSV(const std::string &filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Impossible to open file : " + filepath);
    }

    std::vector<Candle> candles;
    candles.reserve(100000);

    std::string line;
    bool isHeader = true;
    int lineNum = 0;

    while (std::getline(file, line)) {
        ++lineNum;
        line = trim(line);

        if (line.empty()) {
            continue;
        }

        if (isHeader) {
            isHeader = false;
            continue;
        }

        std::stringstream ss(line);
        std::string token;
        Candle candle;

        try {
            std::getline(ss, token, ',');
            candle.timestamp = trim(token);
            std::getline(ss, token, ',');
            candle.open = std::stod(trim(token));
            std::getline(ss, token, ',');
            candle.high = std::stod(trim(token));
            std::getline(ss, token, ',');
            candle.low = std::stod(trim(token));
            std::getline(ss, token, ',');
            candle.close = std::stod(trim(token));
            std::getline(ss, token, ',');
            candle.volume = std::stod(trim(token));

            candles.push_back(std::move(candle));
        } catch (const std::exception &e) {
            std::cerr << "[DataParser] Line " << lineNum << " ignored (parsing error) : " << e.what() << "\n";
        }
    }

    std::cout << "[DataParser] " << candles.size() << " candles loaded from " << filepath << "\n";
    return candles;
}
