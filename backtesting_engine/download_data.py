import yfinance as yf
import sys

# Parameters (can be overridden by command-line arguments)
symbol    = sys.argv[1] if len(sys.argv) > 1 else "BTC-USD"
period    = sys.argv[2] if len(sys.argv) > 2 else "2y"      # 2 years
interval  = sys.argv[3] if len(sys.argv) > 3 else "1d"      # daily candles
output    = sys.argv[4] if len(sys.argv) > 4 else "data/btc.csv"

print(f"Downloading {symbol} ({period}, {interval})...")

df = yf.download(symbol, period=period, interval=interval)
df = df[["Open", "High", "Low", "Close", "Volume"]]
df.index.name = "timestamp"
df.columns    = ["open", "high", "low", "close", "volume"]

df.to_csv(output)
print(f"{len(df)} candles saved to {output}")
