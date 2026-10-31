# Samy-NASSET

Personal software engineering projects, experiments and technical explorations.

This repository is a collection of projects built to practice and demonstrate skills in:

- C++ and Python
- Software architecture
- Algorithm design and performance optimization
- Quantitative finance
- Data processing
- LLM applications (RAG, MCP)

Each project is self-contained and includes its own documentation, build instructions and usage examples.

## Projects

| Project | Stack | Description |
|---------|-------|-------------|
| [`backtesting_engine`](backtesting_engine/README.md) | C++17, CMake | Modular financial backtesting engine to simulate and optimize algorithmic trading strategies on historical data |
| [`nexus-ai`](nexus-ai/README.md) | Python, FastAPI, Streamlit, Ollama, ChromaDB, MCP | Fully local AI assistant with streaming chat, document Q&A (RAG) and an MCP server |

## Quick start

All commands target **Linux / WSL (Ubuntu)**. On Windows, run them inside WSL.

### C++ Backtesting Engine

```bash
git clone https://github.com/Slymoz/Samy-NASSET.git
cd Samy-NASSET/backtesting_engine

./build.sh
python3 download_data.py BTC-USD 2y 1d data/btc.csv
./build/backtest data/btc.csv ma 10 50
```

### Nexus AI (Python, runs in a virtual environment)

Prerequisite: [Ollama](https://ollama.com) with `ollama pull llama3.2:1b` and `ollama pull nomic-embed-text`.

```bash
cd Samy-NASSET/nexus-ai

python3 -m venv .venv          # isolated Python environment
source .venv/bin/activate
pip install -r requirements.txt

./start.sh                     # UI on http://localhost:8501
```

See the [Nexus AI README](nexus-ai/README.md) for configuration, MCP setup and troubleshooting.

## Continuous integration

Two GitHub Actions workflows run on every push and pull request:

- **C++ CI**: builds the backtesting engine and runs the lint checks.
- **Nexus AI CI**: installs the dependencies in a venv, runs `ruff` and `pytest`, then starts the full stack with `start.sh` and checks that the API, the web UI and the MCP server respond.

## License

Unless stated otherwise, projects in this repository are released under the MIT License.
