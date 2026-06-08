# C++ Financial Backtesting Engine

A modular and high-performance C++17 backtesting engine designed to simulate, evaluate and optimize algorithmic trading strategies on historical market data.

Built entirely from scratch in modern C++, without external financial libraries, this project demonstrates object-oriented design, quantitative analysis, risk management and automated strategy optimization.

## 🚀 Features

* **CSV Market Data Ingestion** — Load OHLCV market data from any CSV source (see data folder for format)
* **Modular Strategy Interface** — Easily create and plug in new trading strategies
* **Technical Indicators** — SMA, EMA and RSI implemented from scratch
* **Risk Management** — Stop-Loss and Take-Profit support for every strategy
* **Realistic Simulation** — Fee-aware execution with detailed trade tracking
* **Portfolio Management** — Cash, position and performance monitoring
* **Parameter Optimizer** — Automatically evaluate hundreds of configurations
* **Financial Metrics** — ROI, Max Drawdown, Win Rate, Profit Factor, Average Win/Loss
* **Code Quality Tooling** — Integrated clang-format and clang-tidy workflow

## 📈 Included Trading Strategies

### Moving Average Crossover (MA)

This strategy compares two moving averages :

* A short-term moving average that reacts quickly to price changes
* A long-term moving average representing the broader market trend

A **BUY** signal is generated when the short-term average crosses above the long-term average.

A **SELL** signal is generated when the short-term average crosses below the long-term average.

This is one of the most widely used trend-following strategies in quantitative trading.

### Relative Strength Index (RSI)

The RSI is a momentum indicator ranging from 0 to 100.

Typical thresholds :

* RSI < 30 → Market considered oversold
* RSI > 70 → Market considered overbought

The strategy attempts to buy when the market appears oversold and sell when it appears overbought.

This approach is commonly used to identify potential trend reversals.

## 📊 Results (BTC/USD — 2 Years Daily Data)

### Parameter Optimization — MA Crossover (460 configurations tested in ~13 ms)

| Configuration | ROI | Max DD | Win Rate | Profit Factor | Trades |
|---------------|-----|--------|----------|---------------|--------|
| **MA(15/100) SL=7% TP=30%** | **+52.38%** | **17.94%** | 40% | **2.70** | 5 |
| MA(5/50) SL=3% | +53.29% | 23.54% | 41.67% | 2.14 | 12 |
| MA(30/100) SL=5% | +37.00% | 18.84% | 50.00% | 2.66 | 4 |
| MA(10/50) No Risk Management | +30.65% | 36.79% | 44.44% | 1.48 | 9 |

> Best configuration : **MA(15/100) SL=7% TP=30%** — ROI +52.38% with only 17.94% drawdown.

## 🛠️ Project Architecture

```text
.
├── include/
│   ├── DataParser.hpp
│   ├── Indicators.hpp
│   ├── Optimizer.hpp
│   ├── Portfolio.hpp
│   ├── Strategy.hpp
│   └── Types.hpp
├── src/
│   ├── DataParser.cpp
│   ├── Indicators.cpp
│   ├── Optimizer.cpp
│   ├── Portfolio.cpp
│   └── strategies/
│       ├── MovingAverageCrossover.hpp
│       └── RSIStrategy.hpp
├── data/
│   ├── btc.csv
├── .clang-format
├── .clang-tidy
├── build.sh
├── lint.sh
├── CMakeLists.txt
├── download_data.py
├── main.cpp
└── README.md
```

### Key Design Decisions

#### Strategy Pattern

Each strategy inherits from the abstract `Strategy` base class and implements :

```cpp
prepare()
evaluate()
```

Adding a new strategy requires no modification to the engine core.

#### Separation of Concerns

* Data ingestion
* Indicator calculation
* Portfolio management
* Strategy logic
* Optimization

are all isolated into dedicated modules.

#### Risk-Aware Evaluation

Strategies can use :

* Stop-Loss
* Take-Profit
* Trading fees

to produce more realistic simulations.

#### Optimization Engine

Parameter combinations are automatically evaluated and ranked using :

```text
(ROI × ProfitFactor) / (1 + MaxDrawdown)
```

This scoring function balances profitability and risk.

## 📋 Requirements

### Linux (Ubuntu / Debian)

Install the required dependencies :

```bash
sudo apt update

sudo apt install \
    build-essential \
    cmake \
    clang-format \
    clang-tidy \
    python3 \
    python3-pip
```

Install the **optional** Python dependency used for market data downloads :

```bash
python3 -m pip install yfinance
```

### Supported Versions

* GCC ≥ 9 or Clang ≥ 10
* CMake ≥ 3.16
* Python ≥ 3.8

## ⚙️ Build

Clone the repository :

```bash
git clone https://github.com/Slymoz/Samy-NASSET.git
cd Samy-NASSET/backtesting_engine
```

Build the project :

```bash
./build.sh
```

The executable will be generated at :

```bash
./build/backtest
```

## 🧹 Code Formatting & Linting

Apply the project coding style :

```bash
./lint.sh
```

This command :

* Applies clang-tidy fixes
* Automatically adds missing braces around conditionals
* Formats source files with clang-format

## 📥 Download Historical Data

From the project root :

```bash
# BTC/USD — 2 years daily candles
python3 download_data.py BTC-USD 2y 1d data/btc.csv

# ETH/USD — 1 year hourly candles
python3 download_data.py ETH-USD 1y 1h data/eth.csv
```

## 🧠 Backtest vs Optimizer

The engine provides two different execution modes.

### Standard Backtest

Runs a single strategy configuration on historical market data and generates a detailed performance report.

Example :

```bash
./build/backtest data/btc.csv ma 10 50
```

This command evaluates :

- Moving Average strategy
- Short period = 10
- Long period = 50

and produces :

- Trade history
- ROI
- Max Drawdown
- Win Rate
- Profit Factor
- Portfolio evolution

This mode is useful when you already know which parameters you want to test.

### Parameter Optimization

Automatically evaluates hundreds of strategy configurations and ranks them according to their risk-adjusted performance.

Example :

```bash
./build/backtest data/btc.csv optimize ma
```

The optimizer :

- Generates parameter combinations
- Runs a full backtest for each configuration
- Computes performance metrics
- Ranks the results

For the Moving Average strategy, the optimizer currently evaluates :

```text
460 configurations
```

in only a few milliseconds.

This mode is useful for discovering the best-performing parameters on a given dataset.

## ▶️ Usage

### Moving Average Crossover

```bash
./build/backtest data/btc.csv ma 10 50
```

With Stop-Loss :

```bash
./build/backtest data/btc.csv ma 10 50 --sl 5
```

With Stop-Loss and Take-Profit :

```bash
./build/backtest data/btc.csv ma 10 50 --sl 5 --tp 20
```

### RSI Strategy

```bash
./build/backtest data/btc.csv rsi 14 30 70
```

With Stop-Loss :

```bash
./build/backtest data/btc.csv rsi 14 30 70 --sl 3
```

## 🔍 Parameter Optimization

Optimize Moving Average parameters :

```bash
./build/backtest data/btc.csv optimize ma
```

Optimize RSI parameters :

```bash
./build/backtest data/btc.csv optimize rsi
```

## 📄 Example Output

```text
=== Simulation : MA Crossover (short=10 / long=50) ===

[BUY]  2024-09-19 | Price: 62940.46 | Qty: 0.158722 | Fees: 9.9900
[SELL] 2024-12-30 | Price: 92643.21 | Qty: 0.158722 | Fees: 14.7045 | PnL: +4689.77$

...

+--------------------------------------+
|           BACKTEST REPORT            |
+--------------------------------------+
| Initial Capital    :      $10000.00  |
| Final Capital      :      $13065.13  |
| ROI                :         30.65%  |
| Max Drawdown       :         36.79%  |
| Win Rate           :         44.44%  |
+--------------------------------------+
| Total Trades       :              9  |
| Winning Trades     :              4  |
| Losing Trades      :              5  |
+--------------------------------------+
| P&L net total      :       $3065.13  |
| Average Win        :       $2352.27  |
| Average Loss       :       $1268.79  |
| Profit Factor      :           1.48  |
+--------------------------------------+
```

## ➕ Adding a Custom Strategy

Create a new file inside :

```text
src/strategies/
```

Example :

```cpp
#pragma once

#include "../../include/Strategy.hpp"

class MyStrategy : public Strategy {

public:
    void prepare(const std::vector<Candle> &candles) override
    {
        // Precompute indicators
    }

    std::optional<Order> evaluate(
        const std::vector<Candle> &candles,
        int index,
        double currentCash,
        double currentPosition) override
    {
        return std::nullopt;
    }

    std::string name() const override
    {
        return "My Strategy";
    }
};
```

Then register it in `main.cpp`.

No modifications to the backtesting engine are required.

## 💡 Technical Skills Demonstrated

- Modern C++17
- Object-Oriented Design
- Strategy Pattern
- STL (vector, optional, algorithms)
- Financial Data Processing
- Performance Optimization
- Risk Management Modeling
- CMake Build System
- clang-format / clang-tidy Integration
- Modular Software Architecture

## ⚠️ Disclaimer

This project is intended for educational purposes only.

Backtested performance does not guarantee future results and should not be considered financial advice.

## 📄 License

MIT License — see the LICENSE file for details.
