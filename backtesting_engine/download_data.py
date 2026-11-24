"""Download OHLCV candles from Yahoo Finance into the CSV format of the engine.

Usage:
    python3 download_data.py [SYMBOL] [PERIOD] [INTERVAL] [OUTPUT]

Examples:
    python3 download_data.py BTC-USD 2y 1d data/btc.csv
    python3 download_data.py ETH-USD 1y 1h data/eth.csv
"""

import argparse
import sys
from pathlib import Path

try:
    import yfinance as yf
except ImportError:  # pragma: no cover
    sys.exit("yfinance is not installed: python3 -m pip install -r requirements.txt")

COLUMNS = ["Open", "High", "Low", "Close", "Volume"]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("symbol", nargs="?", default="BTC-USD", help="ticker (default: BTC-USD)")
    parser.add_argument("period", nargs="?", default="2y", help="history length, e.g. 1y, 2y, max")
    parser.add_argument("interval", nargs="?", default="1d", help="candle size, e.g. 1d, 1h")
    parser.add_argument("output", nargs="?", default="data/btc.csv", help="CSV file to write")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    print(f"Downloading {args.symbol} ({args.period}, {args.interval})...")

    df = yf.download(
        args.symbol,
        period=args.period,
        interval=args.interval,
        auto_adjust=False,
        progress=False,
    )
    if df is None or df.empty:
        print(f"No data returned for {args.symbol}.", file=sys.stderr)
        return 1

    # Recent yfinance versions return (field, ticker) column pairs.
    if df.columns.nlevels > 1:
        df.columns = df.columns.get_level_values(0)

    df = df[COLUMNS].dropna()
    df.index.name = "timestamp"
    df.columns = [c.lower() for c in COLUMNS]

    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    df.to_csv(output)
    print(f"{len(df)} candles saved to {output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
