# download_data.py
import yfinance as yf
import sys

# Paramètres
symbol    = sys.argv[1] if len(sys.argv) > 1 else "BTC-USD"
period    = sys.argv[2] if len(sys.argv) > 2 else "2y"      # 2 ans
interval  = sys.argv[3] if len(sys.argv) > 3 else "1d"      # bougies journalières
output    = sys.argv[4] if len(sys.argv) > 4 else "data/btc.csv"

print(f"Téléchargement de {symbol} ({period}, {interval})...")

df = yf.download(symbol, period=period, interval=interval)
df = df[["Open", "High", "Low", "Close", "Volume"]]
df.index.name = "timestamp"
df.columns    = ["open", "high", "low", "close", "volume"]

df.to_csv(output)
print(f"{len(df)} bougies sauvegardées dans {output}")
