# Changelog

## [2.0.0] - 2026-11-24

### Fixed
- Profit factor was 0 when a run had no losing trade, which ranked perfect configurations last in the optimizer. It is now +inf (capped at 10 in the ranking score).
- Out-of-bounds writes in `EMA` and `RSI` when the price series was not longer than the period.
- RSI returned 100 on a flat market; it now returns 50.
- `lint.sh` could never fail the CI (`|| true`, in-place fixes): it now has a `--check` mode.
- Command-line parsing: options mixed with positional arguments crashed (`ma 10 --sl 5`), invalid numbers were accepted or threw uncaught exceptions.
- Strategy and portfolio could disagree on the open position (state was tracked twice).
- The optimizer kept a reference to the candles (possible dangling reference).

### Changed
- **Execution model**: signals computed on a bar's close are now filled at the next bar's open (no look-ahead). Results differ from 1.x.
- Stop-loss / take-profit are triggered on the bar's low / high and filled at the stop / target price, or at the open on a gap. Stop-loss wins when both are hit in the same bar.
- Strategies only emit signals (`Signal::BUY / SELL / NONE`); sizing, costs and risk management moved to the new `Backtester` (replaces `Portfolio`).
- Strategies moved to `include/strategies` and `src/strategies` (headers + sources).
- Indicators return NaN (not 0) for values that cannot be computed yet.
- The engine no longer prints anything: console output and exports live in `Report`.
- The CSV parser locates columns by header name, validates rows (non-positive prices, inconsistent high/low) and sorts candles chronologically.
- CMake: `backtest_core` library + `backtest` executable, warnings enabled in every build type, `BACKTEST_WARNINGS_AS_ERRORS` and `BACKTEST_SANITIZERS` options.

### Added
- Slippage (`--slippage`, default 0.05 %), configurable capital and fees (`--capital`, `--fee`).
- Metrics: CAGR, Sharpe, Sortino, exposure and buy & hold benchmark.
- Optimizer train/test split (`--train`, default 0.7) with out-of-sample results.
- Multi-threaded optimizer (`--threads`) with indicators computed once per parameter set.
- EMA crossover (`--ema`).
- CSV export of the equity curve and trades (`--export`).
- Bar frequency inferred from the timestamps (`--periods-per-year` to override).
- GoogleTest unit tests (indicators, engine, metrics, parser, CLI, strategies, optimizer).
- CI: lint check, GCC and Clang builds with `-Werror`, sanitizer build, CLI smoke tests.
- `requirements.txt` for the download script.

## [1.0.0]

- Initial version: MA crossover and RSI strategies, stop-loss / take-profit, grid-search optimizer.
