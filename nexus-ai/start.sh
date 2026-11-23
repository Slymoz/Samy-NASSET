#!/usr/bin/env bash
# Start the whole Nexus AI stack (API + web UI + MCP server).
# Expects a virtual environment in ./.venv (see README, "Quick start").
set -Eeuo pipefail
cd "$(dirname "$0")"

# Optional local settings (see .env.example). Variables already set win.
if [[ -f .env ]]; then
    while IFS='=' read -r key value; do
        key="${key//[[:space:]]/}"
        if [[ -z "$key" || "$key" == \#* ]]; then continue; fi
        value="${value%$'\r'}"                       # tolerate CRLF files
        [[ "$value" =~ ^\"(.*)\"$ || "$value" =~ ^\'(.*)\'$ ]] && value="${BASH_REMATCH[1]}"
        if [[ -z "${!key:-}" ]]; then export "$key=$value"; fi
    done < .env
fi

HOST="${NEXUS_HOST:-127.0.0.1}"   # 0.0.0.0 inside Docker
API_PORT="${NEXUS_API_PORT:-8000}"
UI_PORT="${NEXUS_UI_PORT:-8501}"
MCP_PORT="${NEXUS_MCP_PORT:-8001}"
OLLAMA_URL="${NEXUS_OLLAMA_URL:-http://127.0.0.1:11434}"

if [[ ! -f .venv/bin/activate ]]; then
    echo "No virtual environment found. Create one first:" >&2
    echo "  python3 -m venv .venv && source .venv/bin/activate" >&2
    echo "  pip install -r requirements.txt" >&2
    exit 1
fi
# shellcheck disable=SC1091
source .venv/bin/activate

# Fail early with a clear message instead of a half-started stack.
for port in "$API_PORT" "$UI_PORT" "$MCP_PORT"; do
    if (echo >"/dev/tcp/127.0.0.1/$port") 2>/dev/null; then
        echo "Port $port is already in use (is Nexus already running?)." >&2
        echo "Stop the other process or set NEXUS_API_PORT / NEXUS_UI_PORT / NEXUS_MCP_PORT." >&2
        exit 1
    fi
done

# Variables read by the Python services.
export NEXUS_MCP_HOST="${NEXUS_MCP_HOST:-$HOST}"
export NEXUS_MCP_PORT="$MCP_PORT"
export NEXUS_MCP_URL="http://127.0.0.1:${MCP_PORT}/mcp"
export NEXUS_API_URL="http://127.0.0.1:${API_PORT}"

# Ollama is only needed for chat and indexing, so warn instead of failing.
if command -v curl >/dev/null && ! curl -sf "${OLLAMA_URL}/api/tags" >/dev/null; then
    echo "Warning: Ollama is not reachable at ${OLLAMA_URL} (run 'ollama serve')."
fi

PIDS=()

cleanup() {
    trap - EXIT INT TERM
    echo ""
    echo "Stopping Nexus AI..."
    for pid in "${PIDS[@]}"; do
        kill "$pid" 2>/dev/null || true
    done
    wait 2>/dev/null || true
    echo "Nexus AI stopped."
}

trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

echo "Starting FastAPI..."
uvicorn app.main:app --host "$HOST" --port "$API_PORT" &
PIDS+=("$!")

echo "Starting Streamlit..."
# --server.headless skips the first-run e-mail prompt (needed in CI).
streamlit run frontend/streamlit_app.py \
    --server.address "$HOST" \
    --server.port "$UI_PORT" \
    --server.headless true \
    --browser.gatherUsageStats false &
PIDS+=("$!")

echo "Starting MCP server..."
python -m app.mcp_server &
PIDS+=("$!")

cat <<MSG

Nexus AI is starting.
  Interface : http://localhost:${UI_PORT}
  MCP page  : http://localhost:${UI_PORT}/MCP
  API       : http://localhost:${API_PORT}  (docs: /docs)
  MCP       : http://127.0.0.1:${MCP_PORT}/mcp
Press Ctrl+C to stop all services.

MSG

# If any service dies, stop everything instead of running half a stack.
wait -n || true
echo "A service stopped unexpectedly." >&2
exit 1
