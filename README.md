# Samy-NASSET

Personal software engineering projects, experiments and technical explorations.

This repository serves as a collection of projects developed to practice and demonstrate skills in :

* C++
* Software architecture
* Algorithm design
* Performance optimization
* Quantitative finance
* Data processing

Each project is self-contained and includes its own documentation, build instructions and usage examples.

## Projects

### Backtesting Engine

A modular C++17 financial backtesting engine used to simulate and optimize algorithmic trading strategies on historical market data.

#### Documentation

- 📖 [Project README](./backtesting_engine/README.md)

#### Quick Start

```bash
git clone https://github.com/Slymoz/Samy-NASSET.git
cd Samy-NASSET/backtesting_engine

./build.sh

python3 download_data.py BTC-USD 2y 1d data/btc.csv

./build/backtest data/btc.csv ma 10 50
```

## License

Unless stated otherwise, projects in this repository are released under the MIT License.
