#!/usr/bin/env bash
# Start the full stack with start.sh and check that every service answers.
# Ollama is NOT required: only the parts that do not need a model are tested.
set -Eeuo pipefail
cd "$(dirname "$0")/.."

# shellcheck disable=SC1091
source .venv/bin/activate

LOG="${SMOKE_LOG:-start.log}"
./start.sh >"$LOG" 2>&1 &
START_PID=$!

cleanup() {
    kill "$START_PID" 2>/dev/null || true   # start.sh then stops its children
    wait "$START_PID" 2>/dev/null || true
}
trap cleanup EXIT

wait_for() {
    local name="$1" url="$2"
    for _ in $(seq 1 90); do
        if curl -sf "$url" >/dev/null; then
            echo "OK  $name"
            return 0
        fi
        if ! kill -0 "$START_PID" 2>/dev/null; then
            echo "FAIL $name: start.sh exited early"
            return 1
        fi
        sleep 1
    done
    echo "FAIL $name: no answer from $url"
    return 1
}

wait_for "FastAPI"                    http://127.0.0.1:8000/api/health
wait_for "Streamlit"                  http://127.0.0.1:8501/_stcore/health
wait_for "Document index (ChromaDB)"  http://127.0.0.1:8000/api/documents
wait_for "Conversations (SQLite)"     http://127.0.0.1:8000/api/conversations

python scripts/check_mcp.py

# The API should now see the MCP server too.
curl -sf http://127.0.0.1:8000/api/status | python -c \
    "import json, sys; s = json.load(sys.stdin); assert s['api'] and s['mcp'], s"
echo "OK  /api/status reports API and MCP up"

echo "Smoke test passed."
