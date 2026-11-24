#include "Optimizer.hpp"

#include "strategies/RSIStrategy.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <exception>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace {

constexpr double PF_CAP = 10.0;
constexpr double NOT_RANKED = -1e9;

std::string riskSuffix(double sl, double tp) {
    std::ostringstream out;
    if (sl > 0.0) {
        out << " SL=" << sl << "%";
    }
    if (tp > 0.0) {
        out << " TP=" << tp << "%";
    }
    return out.str();
}

// Run `count` independent jobs on `threads` threads; rethrow the first error.
void parallelFor(std::size_t count, unsigned threads, const std::function<void(std::size_t)> &job) {
    std::atomic<std::size_t> next{0};
    std::exception_ptr error;
    std::mutex errorMutex;

    auto worker = [&]() {
        for (std::size_t i = next++; i < count; i = next++) {
            try {
                job(i);
            } catch (...) {
                const std::lock_guard<std::mutex> lock(errorMutex);
                if (!error) {
                    error = std::current_exception();
                }
            }
        }
    };

    const unsigned n = std::max(1u, std::min<unsigned>(threads, static_cast<unsigned>(count)));
    std::vector<std::thread> pool;
    pool.reserve(n - 1);
    for (unsigned t = 1; t < n; ++t) {
        pool.emplace_back(worker);
    }
    worker();
    for (auto &thread : pool) {
        thread.join();
    }
    if (error) {
        std::rethrow_exception(error);
    }
}

} // namespace

Optimizer::Optimizer(std::vector<Candle> candles, BacktestConfig baseConfig, OptimizerOptions options)
    : m_candles(std::move(candles)),
      m_baseConfig(baseConfig),
      m_options(options),
      m_split(0) {
    if (m_candles.empty()) {
        throw std::invalid_argument("No data to optimize on.");
    }
    if (!(m_options.trainRatio > 0.0 && m_options.trainRatio <= 1.0)) {
        throw std::invalid_argument("The train ratio must be in (0, 1]");
    }
    if (m_options.threads == 0) {
        m_options.threads = std::max(1u, std::thread::hardware_concurrency());
    }
    const auto bars = static_cast<double>(m_candles.size());
    m_split = static_cast<std::size_t>(std::floor(bars * m_options.trainRatio));
    m_split = std::clamp<std::size_t>(m_split, 1, m_candles.size());
}

double Optimizer::score(const BacktestResult &r, int minTrades) {
    if (r.totalTrades < minTrades) {
        return NOT_RANKED;
    }
    if (r.roi <= 0.0) {
        return r.roi;
    }
    return r.roi * std::min(r.profitFactor, PF_CAP) / (1.0 + r.maxDrawdown);
}

std::vector<OptimResult> Optimizer::run(const std::vector<StrategyFactory> &factories, std::vector<double> stopLosses,
                                        std::vector<double> takeProfits) const {
    if (stopLosses.empty()) {
        stopLosses.push_back(0.0);
    }
    if (takeProfits.empty()) {
        takeProfits.push_back(0.0);
    }

    // Indicators are computed once per parameter set, then shared (read-only)
    // by every stop-loss / take-profit variant.
    std::vector<std::unique_ptr<Strategy>> strategies(factories.size());
    parallelFor(factories.size(), m_options.threads, [&](std::size_t i) {
        strategies[i] = factories[i]();
        strategies[i]->prepare(m_candles);
    });

    const std::size_t perStrategy = stopLosses.size() * takeProfits.size();
    std::vector<OptimResult> results(strategies.size() * perStrategy);
    const bool hasTest = m_split < m_candles.size();

    parallelFor(results.size(), m_options.threads, [&](std::size_t job) {
        const Strategy &strategy = *strategies[job / perStrategy];
        const double sl = stopLosses[(job % perStrategy) / takeProfits.size()];
        const double tp = takeProfits[job % takeProfits.size()];

        BacktestConfig config = m_baseConfig;
        config.stopLossPct = sl;
        config.takeProfitPct = tp;
        const Backtester backtester(config);

        OptimResult &out = results[job];
        out.label = strategy.name() + riskSuffix(sl, tp);
        out.stopLossPct = sl;
        out.takeProfitPct = tp;
        out.train = backtester.simulate(m_candles, strategy, 0, m_split);
        out.score = score(out.train, m_options.minTrades);
        if (hasTest) {
            out.test = backtester.simulate(m_candles, strategy, m_split, m_candles.size());
        }
        // The curves are not needed for the ranking: keep memory low.
        out.train.equityCurve.clear();
        out.train.trades.clear();
        if (out.test) {
            out.test->equityCurve.clear();
            out.test->trades.clear();
        }
    });

    std::stable_sort(results.begin(), results.end(),
                     [](const OptimResult &a, const OptimResult &b) { return a.score > b.score; });
    return results;
}

std::vector<OptimResult> Optimizer::runMA(const MaGrid &grid) const {
    std::vector<StrategyFactory> factories;
    for (std::size_t shortP : grid.shortPeriods) {
        for (std::size_t longP : grid.longPeriods) {
            if (shortP == 0 || shortP >= longP) {
                continue;
            }
            const MaType type = grid.type;
            factories.emplace_back([=]() { return std::make_unique<MovingAverageCrossover>(shortP, longP, type); });
        }
    }
    return run(factories, grid.stopLosses, grid.takeProfits);
}

std::vector<OptimResult> Optimizer::runRSI(const RsiGrid &grid) const {
    std::vector<StrategyFactory> factories;
    for (std::size_t period : grid.periods) {
        for (double oversold : grid.oversolds) {
            for (double overbought : grid.overboughts) {
                if (period == 0 || oversold >= overbought) {
                    continue;
                }
                factories.emplace_back([=]() { return std::make_unique<RSIStrategy>(period, oversold, overbought); });
            }
        }
    }
    return run(factories, grid.stopLosses, grid.takeProfits);
}
