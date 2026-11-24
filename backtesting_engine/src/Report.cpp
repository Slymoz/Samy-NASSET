#include "Report.hpp"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace {

std::string pct(double value) {
    return Report::formatNumber(value) + "%";
}

std::string money(double value) {
    return (value < 0.0 ? "-$" : "$") + Report::formatNumber(std::fabs(value));
}

std::string line(std::size_t width, char fill = '-') {
    return "+" + std::string(width, fill) + "+\n";
}

std::string fit(std::string text, std::size_t width) {
    if (text.size() > width) {
        text = text.substr(0, width);
    }
    return text;
}

std::ofstream openForWriting(const std::string &path) {
    std::ofstream file(path);
    if (!file) {
        throw std::runtime_error("Cannot write file: " + path);
    }
    file << std::setprecision(10);
    return file;
}

} // namespace

namespace Report {

std::string formatNumber(double value, int precision) {
    if (std::isinf(value)) {
        return value > 0 ? "inf" : "-inf";
    }
    if (std::isnan(value)) {
        return "n/a";
    }
    std::ostringstream out;
    out << std::fixed << std::setprecision(precision) << value;
    return out.str();
}

void printSummary(std::ostream &out, const BacktestResult &r, const std::string &title) {
    constexpr std::size_t LABEL_W = 18;
    constexpr std::size_t VALUE_W = 14;
    constexpr std::size_t INNER_W = LABEL_W + VALUE_W + 5;

    auto row = [&](const std::string &label, const std::string &value) {
        out << "| " << std::left << std::setw(static_cast<int>(LABEL_W)) << label << " : " << std::right
            << std::setw(static_cast<int>(VALUE_W)) << value << " |\n";
    };
    auto centered = [&](const std::string &text) {
        const std::string t = fit(text, INNER_W);
        const std::size_t left = (INNER_W - t.size()) / 2;
        out << "|" << std::string(left, ' ') << t << std::string(INNER_W - t.size() - left, ' ') << "|\n";
    };

    out << "\n" << line(INNER_W);
    centered("BACKTEST REPORT");
    centered(title);
    out << line(INNER_W);
    row("Initial Capital", money(r.initialCapital));
    row("Final Capital", money(r.finalCapital));
    row("ROI", pct(r.roi));
    row("Buy & Hold ROI", pct(r.buyHoldRoi));
    row("CAGR", pct(r.cagr));
    row("Max Drawdown", pct(r.maxDrawdown));
    row("Sharpe", formatNumber(r.sharpe));
    row("Sortino", formatNumber(r.sortino));
    row("Exposure", pct(r.exposure));
    out << line(INNER_W);
    row("Total Trades", std::to_string(r.totalTrades));
    row("Winning Trades", std::to_string(r.winningTrades));
    row("Losing Trades", std::to_string(r.losingTrades));
    row("Win Rate", pct(r.winRate));
    out << line(INNER_W);
    row("Net P&L", money(r.totalPnl));
    row("Average Win", money(r.avgWin));
    row("Average Loss", money(r.avgLoss));
    row("Profit Factor", formatNumber(r.profitFactor));
    out << line(INNER_W);
}

void printTrades(std::ostream &out, const std::vector<TradeRecord> &trades) {
    if (trades.empty()) {
        out << "\nNo trade.\n";
        return;
    }
    out << "\n"
        << std::left << std::setw(4) << "#" << std::setw(21) << "Entry" << std::setw(21) << "Exit" << std::right
        << std::setw(12) << "Entry px" << std::setw(12) << "Exit px" << std::setw(12) << "P&L" << std::setw(9)
        << "Return" << "  " << std::left << "Exit reason\n";

    std::size_t n = 0;
    for (const auto &t : trades) {
        out << std::left << std::setw(4) << ++n << std::setw(21) << fit(t.entryTimestamp, 20) << std::setw(21)
            << fit(t.exitTimestamp, 20) << std::right << std::setw(12) << formatNumber(t.entryPrice) << std::setw(12)
            << formatNumber(t.exitPrice) << std::setw(12) << formatNumber(t.pnl) << std::setw(9) << pct(t.returnPct)
            << "  " << toString(t.exitReason) << "\n";
    }
}

void printOptimization(std::ostream &out, const std::vector<OptimResult> &results, const std::string &title,
                       std::size_t top) {
    if (results.empty()) {
        out << "[Optimizer] No configuration to test.\n";
        return;
    }

    const bool hasTest = results.front().test.has_value();
    constexpr int C_CONF = 40;
    constexpr int C_NUM = 9;
    const std::size_t columns = hasTest ? 7 : 5;
    const std::size_t innerW = C_CONF + 2 + columns * (C_NUM + 3);

    out << "\n" << line(innerW);
    out << "|" << std::left << std::setw(static_cast<int>(innerW)) << fit(" OPTIMIZATION: " + title, innerW) << "|\n";
    if (hasTest) {
        out << "|" << std::left << std::setw(static_cast<int>(innerW))
            << " Ranked on the train period; Test columns = same parameters on unseen data" << "|\n";
    }
    out << line(innerW);

    out << "| " << std::left << std::setw(C_CONF) << "Configuration" << std::right;
    for (const char *header : {"ROI", "MaxDD", "Sharpe", "PF", "Trades"}) {
        out << " | " << std::setw(C_NUM) << header;
    }
    if (hasTest) {
        out << " | " << std::setw(C_NUM) << "Test ROI" << " | " << std::setw(C_NUM) << "Test B&H";
    }
    out << " |\n" << line(innerW);

    std::size_t shown = 0;
    for (const auto &r : results) {
        if (shown == top) {
            break;
        }
        const std::string marker = shown == 0 ? "* " : "  ";
        out << "| " << std::left << std::setw(C_CONF) << fit(marker + r.label, C_CONF) << std::right;
        out << " | " << std::setw(C_NUM) << pct(r.train.roi) << " | " << std::setw(C_NUM) << pct(r.train.maxDrawdown)
            << " | " << std::setw(C_NUM) << formatNumber(r.train.sharpe) << " | " << std::setw(C_NUM)
            << formatNumber(r.train.profitFactor) << " | " << std::setw(C_NUM) << r.train.totalTrades;
        if (r.test) {
            out << " | " << std::setw(C_NUM) << pct(r.test->roi) << " | " << std::setw(C_NUM)
                << pct(r.test->buyHoldRoi);
        }
        out << " |\n";
        ++shown;
    }
    out << line(innerW);

    const OptimResult &best = results.front();
    out << "\nBest (train): " << best.label << "\n";
    out << "  Train : ROI " << pct(best.train.roi) << " | B&H " << pct(best.train.buyHoldRoi) << " | MaxDD "
        << pct(best.train.maxDrawdown) << " | Sharpe " << formatNumber(best.train.sharpe) << " | Trades "
        << best.train.totalTrades << "\n";
    if (best.test) {
        out << "  Test  : ROI " << pct(best.test->roi) << " | B&H " << pct(best.test->buyHoldRoi) << " | MaxDD "
            << pct(best.test->maxDrawdown) << " | Sharpe " << formatNumber(best.test->sharpe) << " | Trades "
            << best.test->totalTrades << "\n";
    }
}

void writeEquityCsv(const std::string &path, const std::vector<EquityPoint> &equity) {
    auto file = openForWriting(path);
    file << "timestamp,equity\n";
    for (const auto &point : equity) {
        file << point.timestamp << "," << point.equity << "\n";
    }
}

void writeTradesCsv(const std::string &path, const std::vector<TradeRecord> &trades) {
    auto file = openForWriting(path);
    file << "entry_timestamp,exit_timestamp,entry_price,exit_price,quantity,entry_fee,exit_fee,pnl,return_pct,"
            "exit_reason\n";
    for (const auto &t : trades) {
        file << t.entryTimestamp << "," << t.exitTimestamp << "," << t.entryPrice << "," << t.exitPrice << ","
             << t.quantity << "," << t.entryFee << "," << t.exitFee << "," << t.pnl << "," << t.returnPct << ","
             << toString(t.exitReason) << "\n";
    }
}

} // namespace Report
