#!/usr/bin/env bash
# Start the full stack with start.sh and check that every service answers.
# Ollama is NOT required: only the parts that do not need a model are tested.
# With SMOKE_WITH_OLLAMA=1 it also indexes a note and asks a real question.
set -Eeuo pipefail
cd "$(dirname "$0")/.."

# shellcheck disable=SC1091
source .venv/bin/activate

API_PORT="${NEXUS_API_PORT:-8000}"
UI_PORT="${NEXUS_UI_PORT:-8501}"
MCP_PORT="${NEXUS_MCP_PORT:-8001}"
API="http://127.0.0.1:${API_PORT}"
export NEXUS_MCP_URL="http://127.0.0.1:${MCP_PORT}/mcp"

# Never touch real data: use a throw-away data directory.
NEXUS_DATA_DIR="$(mktemp -d)"
export NEXUS_DATA_DIR

LOG="${SMOKE_LOG:-start.log}"
./start.sh >"$LOG" 2>&1 &
START_PID=$!

cleanup() {
    kill "$START_PID" 2>/dev/null || true   # start.sh then stops its children
    wait "$START_PID" 2>/dev/null || true
    rm -r -- "${NEXUS_DATA_DIR:?}"
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

wait_for "FastAPI"                    "$API/api/health"
wait_for "Streamlit"                  "http://127.0.0.1:${UI_PORT}/_stcore/health"
wait_for "Document index (ChromaDB)"  "$API/api/documents"
wait_for "Conversations (SQLite)"     "$API/api/conversations"

python scripts/check_mcp.py

# The API should now see the MCP server too.
curl -sf "$API/api/status" | python -c \
    "import json, sys; s = json.load(sys.stdin); assert s['api'] and s['mcp'], s"
echo "OK  /api/status reports API and MCP up"

# Conversation round trip (no model needed).
CID=$(curl -sf -X POST "$API/api/conversations" | python -c "import json,sys; print(json.load(sys.stdin)['id'])")
curl -sf -X PATCH "$API/api/conversations/$CID" -H 'Content-Type: application/json' \
    -d '{"title": "Smoke test"}' >/dev/null
curl -sf "$API/api/conversations/$CID/export" | grep -q "^# Smoke test"
curl -sf -X DELETE "$API/api/conversations/$CID" >/dev/null
echo "OK  conversation create / rename / export / delete"

if [[ "${SMOKE_WITH_OLLAMA:-0}" == "1" ]]; then
    printf 'The Nexus smoke test codeword is pineapple.\n' >"$NEXUS_DATA_DIR/codeword.txt"
    curl -sf -F "file=@$NEXUS_DATA_DIR/codeword.txt" "$API/api/documents" >/dev/null
    curl -sf "$API/api/search?q=smoke+test+codeword" | grep -q "pineapple"
    echo "OK  indexing and search through Ollama embeddings"
    curl -sf -X POST "$API/api/chat" -H 'Content-Type: application/json' \
        -d '{"message": "What is the smoke test codeword? Answer with one word."}' \
        | python -c "import json,sys; a = json.load(sys.stdin)['answer']; print('    answer:', a.strip()[:80]); assert a.strip()"
    echo "OK  chat answer from ${NEXUS_CHAT_MODEL:-llama3.2:1b}"
    python scripts/check_mcp.py --with-ollama
fi

echo "Smoke test passed."
