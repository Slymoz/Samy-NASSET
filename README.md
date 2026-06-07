# C++ Financial Backtesting Engine

A high-performance, modular C++ backtesting engine for simulating and
evaluating quantitative trading strategies on historical market data.
Built from scratch in C++17 — no external financial libraries.

---

## 🚀 Features

- **CSV Market Data Ingestion** — Loads OHLCV timeseries from any CSV source
- **Modular Strategy Interface** — Implement custom strategies via a clean abstract base class
- **Technical Indicators** — SMA, EMA, RSI computed from scratch
- **Risk Management** — Configurable Stop-Loss and Take-Profit per strategy
- **Realistic Simulation** — Fee-aware order execution, full P&L tracking per trade
- **Parameter Optimizer** — Automatically tests hundreds of configurations and ranks them
- **Financial Metrics** — ROI, Max Drawdown, Win Rate, Profit Factor, Avg Win/Loss

---

## 📊 Results (BTC/USD — 2 years daily)

### Parameter Optimization — MA Crossover (460 configs in 14ms)

| Configuration         | ROI     | Max DD  | Win Rate | Profit Factor | Trades |
|-----------------------|---------|---------|----------|---------------|--------|
| **MA(15/100) SL=7% TP=30%** | **+52.38%** | **17.94%** | 40% | **2.70** | 5 |
| MA(5/50) SL=3%        | +53.29% | 23.54%  | 41.67%   | 2.14          | 12     |
| MA(30/100) SL=5%      | +37.00% | 18.84%  | 50.00%   | 2.66          | 4      |
| MA(10/50) no risk mgmt| +30.65% | 36.79%  | 44.44%   | 1.48          | 9      |

> Best config : **MA(15/100) SL=7% TP=30%** — ROI +52.38% with only 17.94% drawdown

---

## 🛠️ Architecture

```text
BacktestingEngine/
├── include/
│ ├── Types.hpp — Candle, Order, TradeRecord, BacktestResult
│ ├── DataParser.hpp — CSV ingestion
│ ├── Indicators.hpp — SMA, EMA, RSI
│ ├── Strategy.hpp — Abstract base class + Stop-Loss/Take-Profit
│ ├── Portfolio.hpp — Order execution, fees, cash management
│ └── Optimizer.hpp — Parameter space exploration
├── src/
│ ├── DataParser.cpp
│ ├── Indicators.cpp
│ ├── Portfolio.cpp
│ ├── Optimizer.cpp
│ └── strategies/
│ ├── MovingAverageCrossover.hpp
│ └── RSIStrategy.hpp
├── data/
│ └── btc.csv
├── main.cpp
├── CMakeLists.txt
└── download_data.py
```

### Key design decisions

- **Strategy Pattern** — Each strategy inherits from `Strategy` and implements
  `prepare()` and `evaluate()`. Adding a new strategy requires zero changes
  to the engine core.
- **Zero raw pointers** — Strategies are managed via `std::unique_ptr`.
- **Silent mode** — The optimizer runs all backtests with output suppressed,
  only printing the final ranking table.
- **Score function** — Configurations are ranked by a composite score:
  `(ROI × ProfitFactor) / (1 + MaxDrawdown)` which balances return,
  consistency and risk.

---

## 📋 Prerequisites

- C++17 compiler (GCC ≥ 9 or Clang ≥ 10)
- CMake ≥ 3.16
- Python 3 + `yfinance` (optional — for data download only)

---

## ⚙️ Build

```bash
git clone https://github.com/yourname/backtesting-engine.git
cd backtesting-engine

mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

## 📥 Data Download

```bash
# BTC/USD — 2 years, daily candles
python3 download_data.py BTC-USD 2y 1d data/btc.csv

# ETH/USD — 1 year, hourly
python3 download_data.py ETH-USD 1y 1h data/eth.csv
```

## 🔁 Usage

```bash
# MA Crossover — short=10, long=50
./backtest ../data/btc.csv ma 10 50

# With Stop-Loss at -5%
./backtest ../data/btc.csv ma 10 50 --sl 5

# With Stop-Loss -5% and Take-Profit +20%
./backtest ../data/btc.csv ma 10 50 --sl 5 --tp 20

# RSI Strategy
./backtest ../data/btc.csv rsi 14 30 70

# RSI with Stop-Loss -3%
./backtest ../data/btc.csv rsi 14 30 70 --sl 3
```

## Run the optimizer

```bash
# Optimize MA Crossover — tests 460 configurations
./backtest ../data/btc.csv optimize ma

# Optimize RSI — tests 432 configurations
./backtest ../data/btc.csv optimize rsi
```

## Example output

```bash
=== Simulation : MA Crossover (short=10 / long=50) [SL=5%] ===
  [BUY]  2024-09-19 | Prix: 62940.46 | Qté: 0.158722 | Frais: 9.99
  [SELL] 2024-12-30 | Prix: 92643.21 | Qté: 0.158722 | Frais: 14.70 | PnL: +4689.77$
  [BUY]  2025-01-19 | Prix: 101089.61 | Qté: 0.145169 | Frais: 14.67
  [SELL] 2025-02-09 | Prix: 96500.09  | Qté: 0.145169 | Frais: 14.00 | PnL: -694.94$
  ...

╔══════════════════════════════════════╗
║        RAPPORT DE BACKTEST           ║
╠══════════════════════════════════════╣
║  Capital initial  : $      10000.00  ║
║  Capital final    : $      14507.62  ║
║  ROI              :          45.08%  ║
║  Max Drawdown     :          29.81%  ║
║  Win Rate         :          44.44%  ║
╠══════════════════════════════════════╣
║  Trades totaux    :               9  ║
║  Trades gagnants  :               4  ║
║  Trades perdants  :               5  ║
╠══════════════════════════════════════╣
║  P&L net total    : $       4507.62  ║
║  Gain moyen       : $       2370.89  ║
║  Perte moyenne    : $        995.18  ║
║  Profit Factor    :            1.91  ║
╚══════════════════════════════════════╝
```

## ➕ Adding a Custom Strategy

Create a new file in src/strategies/ :

#pragma once
#include "../../include/Strategy.hpp"

class MyStrategy : public Strategy {
public:
    void prepare(const std::vector<Candle>& candles) override {
        // precompute indicators here
    }

    std::optional<Order> evaluate(
        const std::vector<Candle>& candles,
        int index,
        double currentCash,
        double currentPosition
    ) override {
        // return BUY / SELL order or std::nullopt
        return std::nullopt;
    }

    std::string name() const override { return "My Strategy"; }
};

Then register it in main.cpp — no changes to the engine required.

## ⚠️ Disclaimer

This project is for educational purposes only.
Backtested results do not guarantee future performance.
Past performance on historical data is not indicative of real market returns.

## 📄 License

MIT License — see [LICENSE](LICENSE) for details.
