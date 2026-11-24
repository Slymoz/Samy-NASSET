# C++ Financial Backtesting Engine

A modular, tested C++17 backtesting engine to simulate, evaluate and optimize algorithmic trading strategies on historical market data.

Built from scratch in modern C++, without external financial libraries. The engine focuses on **realistic execution** (no look-ahead, intra-bar stops, fees and slippage) and on **honest evaluation** (out-of-sample testing, buy & hold benchmark, risk-adjusted metrics).

## 🚀 Features

* **Realistic execution model** — signals computed on a bar's close are filled at the **next bar's open**; stop-loss / take-profit are triggered on the bar's **low / high** (with gap handling); fees and slippage on every fill
* **Out-of-sample optimization** — parameters are chosen on a train period and evaluated on unseen data (`--train 0.7` by default)
* **Parallel optimizer** — multi-threaded grid search, indicators computed once per parameter set, deterministic results
* **Financial metrics** — ROI, CAGR, Max Drawdown, Sharpe, Sortino, exposure, win rate, profit factor, average win/loss, **buy & hold benchmark**
* **Technical indicators** — SMA, EMA and RSI (Wilder) implemented from scratch
* **Robust CSV ingestion** — columns found by name, invalid rows reported and skipped, automatic chronological sorting, bar frequency inferred from the timestamps
* **Exports** — equity curve and trade list as CSV for plotting
* **Quality tooling** — 50+ GoogleTest unit tests, clang-format / clang-tidy, ASan / UBSan build, CI on GCC and Clang with warnings as errors

## 📈 Included Trading Strategies

### Moving Average Crossover (MA)

Compares a short-term and a long-term moving average (SMA by default, EMA with `--ema`):

* **BUY** when the short average crosses above the long one
* **SELL** when it crosses back below

A classic trend-following strategy.

### Relative Strength Index (RSI)

The RSI is a momentum oscillator ranging from 0 to 100 (typically oversold below 30, overbought above 70):

* **BUY** when the RSI leaves the oversold zone (crosses back above the oversold level)
* **SELL** when it leaves the overbought zone (crosses back below the overbought level)

A mean-reversion strategy.

## ⚙️ Execution Model

For each bar `i`, the engine:

1. fills the signal emitted at the close of bar `i-1` at the **open** of bar `i` (plus slippage and fees);
2. checks the stop-loss against the bar's **low** and the take-profit against its **high**, filling at the stop / target price, or at the open if the bar gapped through it. When both are hit in the same bar, the stop-loss wins (the order of intra-bar moves is unknown, so the engine assumes the worst);
3. marks the portfolio to market at the **close**;
4. lets the strategy see that close and emit a new signal.

An open position is closed at the last close. The strategy is long-only and invests all available cash.

Defaults: capital $10,000, fee 0.1 % per side, slippage 0.05 % per fill.

## 📊 Results (BTC/USD — 2 Years Daily Data)

### Single backtest — MA Crossover (10/50)

| Metric | Value |
|--------|-------|
| ROI | **+29.49%** |
| Buy & Hold ROI | -8.76% |
| CAGR | 13.77% |
| Max Drawdown | 37.10% |
| Sharpe / Sortino | 0.61 / 0.97 |
| Exposure | 47.06% |
| Trades / Win rate | 9 / 44.44% |
| Profit Factor | 1.46 |

### Optimization — MA Crossover (460 configurations in ~10 ms)

Train: 2024-06-08 → 2025-10-31 (70 %) · Test: 2025-11-01 → 2026-06-08 (30 %)

| Configuration | Train ROI | Train MaxDD | Train Sharpe | Trades | **Test ROI** | Test B&H |
|---------------|-----------|-------------|--------------|--------|--------------|----------|
| **MA(15/100) SL=7% TP=30%** | +54.14% | 9.57% | 1.60 | 4 | **-7.23%** | -42.27% |
| MA(15/100) SL=7% | +64.59% | 17.10% | 1.31 | 4 | -7.23% | -42.27% |
| MA(20/100) | +55.52% | 18.01% | 1.16 | 3 | -21.65% | -42.27% |
| MA(5/50) SL=3% | +69.57% | 17.63% | 1.49 | 9 | +0.25% | -42.27% |

> The best in-sample configuration earns +54 % on the train period but loses 7 % on unseen data. It still beats buy & hold (-42 %) in that falling market, but the gap between train and test results is a textbook illustration of **overfitting**: this is exactly why the optimizer reports out-of-sample results.

## 🛠️ Project Architecture

```text
.
├── include/
│   ├── Backtester.hpp          # simulation engine (execution, stops, metrics)
│   ├── Cli.hpp                 # command-line parsing
│   ├── DataParser.hpp          # CSV loading and validation
│   ├── Indicators.hpp          # SMA, EMA, RSI
│   ├── Metrics.hpp             # drawdown, Sharpe, Sortino, CAGR, profit factor
│   ├── Optimizer.hpp           # parallel grid search with train/test split
│   ├── Report.hpp              # console reports and CSV exports
│   ├── Strategy.hpp            # strategy interface
│   ├── Types.hpp               # Candle, Signal, TradeRecord, BacktestConfig/Result
│   └── strategies/
│       ├── MovingAverageCrossover.hpp
│       └── RSIStrategy.hpp
├── src/                        # implementations (same layout)
├── tests/                      # GoogleTest unit tests
├── data/btc.csv
├── CMakeLists.txt
├── build.sh
├── lint.sh
├── download_data.py
├── requirements.txt
└── main.cpp
```

### Key Design Decisions

#### Strategies only emit signals

A strategy decides *when* to be in the market and returns `Signal::BUY`, `Signal::SELL` or `Signal::NONE`. Position sizing, fees, slippage, stop-loss and take-profit belong to the `Backtester`. Strategies therefore hold no trading state: once prepared, a strategy is read-only and can be shared by many simulations running in parallel.

#### No look-ahead by construction

Indicators are causal (the value at bar `i` only uses bars `≤ i`) and signals are executed on the next bar's open, so a strategy can never trade on a price it has not seen yet.

#### Honest optimization

The optimizer ranks configurations on the train period only and reports their performance on the test period. Configurations are ranked with:

```math
score = \frac{ROI \times \min(PF, 10)}{1 + MaxDD}
```

for profitable runs (the profit factor is capped so a lucky trade cannot dominate), by ROI for losing runs, and runs with fewer than 3 trades are not ranked.

#### Library + executable

The engine is built as a static library (`backtest_core`) used by both the `backtest` executable and the unit tests.

## 📋 Requirements

### Linux (Ubuntu / Debian)

```bash
sudo apt update
sudo apt install build-essential cmake clang-format clang-tidy python3 python3-pip
```

GoogleTest is downloaded automatically by CMake the first time the tests are built (or the system package is used if installed).

Optional, to download market data:

```bash
python3 -m pip install -r requirements.txt
```

### Supported Versions

* GCC ≥ 9 or Clang ≥ 10
* CMake ≥ 3.16
* Python ≥ 3.8

## ⚙️ Build & Test

```bash
git clone https://github.com/Slymoz/Samy-NASSET.git
cd Samy-NASSET/backtesting_engine

./build.sh               # Release build + unit tests
./build.sh Debug         # Debug build + unit tests
./build.sh --no-tests    # build only
```

The executable is generated at `./build/backtest`.

Sanitizers (AddressSanitizer + UndefinedBehaviorSanitizer):

```bash
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DBACKTEST_SANITIZERS=ON
cmake --build build-asan && (cd build-asan && ctest --output-on-failure)
```

CMake options: `BACKTEST_BUILD_TESTS` (ON), `BACKTEST_WARNINGS_AS_ERRORS` (OFF), `BACKTEST_SANITIZERS` (OFF).

## 🧹 Code Formatting & Linting

```bash
./lint.sh            # apply clang-tidy fixes and clang-format
./lint.sh --check    # change nothing, fail on any issue (used by the CI)
```

## 📥 Download Historical Data

```bash
# BTC/USD — 2 years of daily candles
python3 download_data.py BTC-USD 2y 1d data/btc.csv

# ETH/USD — 1 year of hourly candles
python3 download_data.py ETH-USD 1y 1h data/eth.csv
```

Any CSV with a header containing `timestamp` (or `date` / `datetime`), `open`, `high`, `low`, `close` and `volume` columns works, in any order.

## ▶️ Usage

```text
backtest <csv> ma  <short> <long>                  [options]
backtest <csv> rsi <period> <oversold> <overbought> [options]
backtest <csv> optimize <ma|rsi>                   [options]
```

| Option | Description | Default |
|--------|-------------|---------|
| `--sl <pct>` | Stop-loss (5 = -5 %) | off |
| `--tp <pct>` | Take-profit (20 = +20 %) | off |
| `--capital <amount>` | Initial capital | 10000 |
| `--fee <pct>` | Fee per side | 0.1 |
| `--slippage <pct>` | Adverse price move per fill | 0.05 |
| `--ema` | MA crossover on EMAs | SMA |
| `--train <ratio>` | Train share for `optimize` (1 = no test period) | 0.7 |
| `--threads <n>` | Optimizer threads | all cores |
| `--periods-per-year <n>` | Bars per year for CAGR / Sharpe | inferred (365 for daily data) |
| `--export <dir>` | Write `equity.csv` and `trades.csv` | — |
| `--quiet` | Hide the trade list | — |

> Stock markets trade about 252 days a year: pass `--periods-per-year 252` for daily stock data.

### Examples

```bash
# Moving average crossover, with stop-loss and take-profit
./build/backtest data/btc.csv ma 10 50 --sl 5 --tp 20

# EMA crossover without costs
./build/backtest data/btc.csv ma 12 26 --ema --fee 0 --slippage 0

# RSI, exporting the equity curve and trades
./build/backtest data/btc.csv rsi 14 30 70 --export results

# Optimization with 80 % of the data for training
./build/backtest data/btc.csv optimize ma --train 0.8

# Optimization with a fixed 5 % stop-loss
./build/backtest data/btc.csv optimize rsi --sl 5
```

When `--sl` / `--tp` are given to `optimize`, that parameter is fixed instead of searched.

## 📄 Example Output

```text
[DataParser] 731 candles loaded from data/btc.csv (2024-06-08 -> 2026-06-08)
[Config] Capital $10000.00 | fee 0.100% | slippage 0.050% | 365 bars/year
[Backtest] MA Crossover (10/50)

#   Entry                Exit                     Entry px     Exit px         P&L   Return  Exit reason
1   2024-09-20           2024-12-31               62972.90    92596.93     4674.87   46.75%  signal
2   2025-01-20           2025-02-10              101134.29    96451.21     -707.49   -4.82%  signal
...

+-------------------------------------+
|           BACKTEST REPORT           |
|        MA Crossover (10/50)         |
+-------------------------------------+
| Initial Capital    :      $10000.00 |
| Final Capital      :      $12949.24 |
| ROI                :         29.49% |
| Buy & Hold ROI     :         -8.76% |
| CAGR               :         13.77% |
| Max Drawdown       :         37.10% |
| Sharpe             :           0.61 |
| Sortino            :           0.97 |
| Exposure           :         47.06% |
+-------------------------------------+
| Total Trades       :              9 |
| Winning Trades     :              4 |
| Losing Trades      :              5 |
| Win Rate           :         44.44% |
+-------------------------------------+
| Net P&L            :       $2949.24 |
| Average Win        :       $2333.17 |
| Average Loss       :       $1276.69 |
| Profit Factor      :           1.46 |
+-------------------------------------+
```

## ➕ Adding a Custom Strategy

Declare it in `include/strategies/MyStrategy.hpp`:

```cpp
#pragma once

#include "Strategy.hpp"

class MyStrategy : public Strategy {
  public:
    Signal onBar(const std::vector<Candle> & /*candles*/, std::size_t index, bool inPosition) const override {
        // Called at the close of bar `index`; executed at the next open.
        if (!inPosition && m_signal[index] > 0.0) {
            return Signal::BUY;
        }
        if (inPosition && m_signal[index] < 0.0) {
            return Signal::SELL;
        }
        return Signal::NONE;
    }

    std::size_t warmup() const override {
        return 20; // first bar at which onBar() may be called
    }

    std::string name() const override {
        return "My Strategy";
    }

  protected:
    void compute(const std::vector<Candle> &candles) override {
        // Precompute causal indicators for the whole series.
        m_signal.assign(candles.size(), 0.0);
    }

  private:
    std::vector<double> m_signal;
};
```

Then register it in `main.cpp` (and in `Optimizer` for grid search). The engine itself does not change. Add unit tests in `tests/` and list new `.cpp` files in `CMakeLists.txt`.

## 💡 Technical Skills Demonstrated

- Modern C++17 (RAII, `std::optional`, `std::filesystem`, move semantics)
- Object-oriented design (Strategy pattern, Non-Virtual Interface)
- Multi-threading (`std::thread`, atomics, deterministic parallel results)
- Quantitative finance (execution modeling, risk metrics, overfitting control)
- Unit testing with GoogleTest
- CMake (library/executable split, options, FetchContent, sanitizers)
- CI on GCC and Clang, clang-format / clang-tidy

## ⚠️ Disclaimer

This project is intended for educational purposes only.

Backtested performance does not guarantee future results and should not be considered financial advice.

## 📄 License

MIT License — see the LICENSE file for details.
