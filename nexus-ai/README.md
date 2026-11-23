# ✳ Nexus AI

![Nexus AI CI/CD](https://github.com/Slymoz/Samy-NASSET/actions/workflows/nexus-ai-ci.yml/badge.svg)
![Python 3.10–3.13](https://img.shields.io/badge/python-3.10%E2%80%933.13-blue)
![License: MIT](https://img.shields.io/badge/license-MIT-green)

A personal AI assistant that runs **100% locally** on top of [Ollama](https://ollama.com):
no paid API, no data leaving your machine.

Nexus combines a chat UI, a **RAG** pipeline (ask questions about your PDFs and
notes) and an **MCP server** so that other AI applications can use your private
knowledge base as a tool.

> Built to understand, end to end, how an LLM application works: model
> integration, streaming, document retrieval, persistence and tool exposure.

## Features

- **Local LLM** through Ollama (default model: `llama3.2:1b`, configurable)
- **Streaming chat** with live token display and speed statistics
- **RAG**: import PDF / TXT / Markdown (several at once), retrieve the most
  relevant passages, show the sources used for each answer. A switch lets you
  chat without your documents.
- **Conversation history** persisted in SQLite, including the sources of each
  answer; rename conversations and export them as Markdown
- **Duplicate detection**: the same file uploaded twice is indexed only once
- **MCP server** exposing 8 tools (search, read, add notes, browse history,
  ask the local LLM), with read-only / destructive hints for MCP clients
- **MCP playground**: a web page to try every MCP tool from the browser, no command line needed
- **Docker**: one `docker compose up` starts Ollama, downloads the models and runs Nexus
- **Tested**: ~80 tests (unit, API, real MCP client, Streamlit UI), 90% coverage,
  plus a smoke test that boots the whole stack, all run in CI on Python 3.10 to 3.13
- **Multilingual**: the assistant answers in the language you write in

## Architecture

```
┌──────────────┐  HTTP/NDJSON   ┌───────────────────┐   HTTP    ┌──────────────┐
│  Streamlit   │ ─────────────▶ │  FastAPI backend  │ ────────▶ │    Ollama    │
│  (frontend)  │                │  app/main.py      │           │ chat + embed │
└──────────────┘                └─────────┬─────────┘           └──────────────┘
                                          │ uses
                       ┌──────────────────┼───────────────────┐
                       ▼                  ▼                   ▼
                  app/rag.py        app/storage.py        app/llm.py
                  ChromaDB          SQLite (history)      prompts + client
                       ▲                  ▲                   ▲
                       └──────────────────┴───────────────────┘
                                          │ same modules
                                ┌─────────┴─────────┐
   Claude Desktop / Code ─────▶ │    MCP server     │
   any MCP client               │ app/mcp_server.py │
                                └───────────────────┘
```

| File | Role |
|------|------|
| `app/config.py` | All settings, overridable with environment variables |
| `app/main.py` | REST API: chat, streaming, conversations, documents, search, status |
| `app/rag.py` | Text extraction, chunking, embeddings, ChromaDB search |
| `app/llm.py` | Prompt building and Ollama chat client |
| `app/storage.py` | SQLite conversation history (with schema migrations) |
| `app/mcp_server.py` | MCP tools for external AI apps |
| `frontend/streamlit_app.py` | Web UI |
| `frontend/pages/1_MCP.py` | Web page to test the MCP tools (a small MCP client) |
| `start.sh` | Starts API + UI + MCP and stops them together |

**Shared index.** The API and the MCP server are two processes using the same
ChromaDB folder. Chroma keeps its vector index in memory per process, so a note
added through MCP used to be invisible to the API (or even crash its searches)
until a restart. `app/rag.py` now guards the index with a file lock and a
version marker: each process reloads its view of the index as soon as the
other one writes.

## Quick start

These steps assume **Linux or WSL (Ubuntu)**. On Windows, run everything inside
WSL. Python projects are run from a **virtual environment** (`venv`) so
dependencies stay isolated from the system Python. Prefer containers? Jump to
[Docker](#docker).

### 1. Install Ollama and the models

```bash
curl -fsSL https://ollama.com/install.sh | sh
ollama serve &                  # skip if Ollama already runs as a service
ollama pull llama3.2:1b         # chat model
ollama pull nomic-embed-text    # embedding model (needed for RAG)
```

### 2. Create the virtual environment and install dependencies

```bash
git clone https://github.com/Slymoz/Samy-NASSET.git
cd Samy-NASSET/nexus-ai

sudo apt update && sudo apt install -y python3 python3-venv python3-pip
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt        # or simply: make install
```

### 3. Start everything

```bash
./start.sh                              # or: make run
```

| Service | URL |
|---------|-----|
| Web interface | http://localhost:8501 |
| MCP playground | http://localhost:8501/MCP |
| API (Swagger docs) | http://localhost:8000/docs |
| MCP server | http://127.0.0.1:8001/mcp |

Press `Ctrl+C` to stop all services. The sidebar shows the status of each one.
On WSL2, open the URLs from your Windows browser (`localhost` is forwarded).

## Docker

The compose file starts Ollama, pulls the two models (first run only) and
starts Nexus. Models and data live in Docker volumes.

```bash
docker compose up -d --build            # or: make docker-up
docker compose logs -f ollama-models    # follow the first model download
```

Then open http://localhost:8501. Ports are published on `127.0.0.1` only.

Already running Ollama on the host? Run the image alone:

```bash
docker build -t nexus-ai:local .
docker run -d --name nexus --add-host=host.docker.internal:host-gateway \
  -e NEXUS_OLLAMA_URL=http://host.docker.internal:11434 \
  -p 127.0.0.1:8501:8501 -p 127.0.0.1:8000:8000 -p 127.0.0.1:8001:8001 \
  -v nexus-data:/data nexus-ai:local
```

(On Linux, Ollama must then listen on more than the loopback:
`OLLAMA_HOST=0.0.0.0 ollama serve`.)

Every push to `main` also publishes the image to the GitHub Container Registry
as `ghcr.io/slymoz/nexus-ai:latest` (and `:<version>` for `nexus-ai-v*` tags).

## Configuration

Environment variables, all optional. Copy `.env.example` to `.env` to set them
permanently: `start.sh` loads it.

| Variable | Default | Purpose |
|----------|---------|---------|
| `NEXUS_CHAT_MODEL` | `llama3.2:1b` | Ollama chat model |
| `NEXUS_EMBED_MODEL` | `nomic-embed-text` | Ollama embedding model |
| `NEXUS_OLLAMA_URL` | `http://127.0.0.1:11434` | Ollama address |
| `NEXUS_LLM_TIMEOUT` | `300` | Seconds to wait for Ollama between two tokens |
| `NEXUS_TEMPERATURE` | *(model default)* | Sampling temperature |
| `NEXUS_HISTORY_MESSAGES` | `20` | Previous messages sent back to the model each turn |
| `NEXUS_CHUNK_SIZE` / `NEXUS_CHUNK_OVERLAP` | `900` / `150` | Passage size and overlap, in characters |
| `NEXUS_TOP_K` | `4` | Passages retrieved per question (max 10) |
| `NEXUS_MAX_DISTANCE` | *(none)* | Ignore passages farther than this cosine distance |
| `NEXUS_MAX_UPLOAD_MB` | `25` | Upload size limit |
| `NEXUS_DATA_DIR` | `./data` | Documents, vector index and SQLite file |
| `NEXUS_HOST` | `127.0.0.1` | Interface the API, UI and MCP server listen on |
| `NEXUS_API_PORT` / `NEXUS_UI_PORT` / `NEXUS_MCP_PORT` | `8000` / `8501` / `8001` | Service ports |

Invalid values (e.g. `NEXUS_TOP_K=abc`) stop the services at startup with a
message naming the variable.

Example with a larger model:

```bash
ollama pull llama3.1:8b
NEXUS_CHAT_MODEL=llama3.1:8b ./start.sh
```

## How RAG works here

1. **Index**: the file is saved, its text extracted (UTF-8 or Windows-1252 text,
   PDF), split into passages of up to 900 characters with a 150-character
   overlap, cut at sentence or word boundaries, embedded with
   `nomic-embed-text` and stored in ChromaDB (cosine distance). A SHA-256 of
   the content detects files that are already indexed.
2. **Retrieve**: the question is embedded and the 4 closest passages are
   fetched (optionally filtered with `NEXUS_MAX_DISTANCE`).
3. **Generate**: the passages are injected into the prompt for that turn only,
   and the model is told to cite file names and not to invent sources. Only
   the last 20 messages of the conversation are sent back to the model.

The UI lists the retrieved passages with their distance (lower is closer), and
keeps them with the answer in the history.

## MCP server

Nexus exposes its knowledge base through the
[Model Context Protocol](https://modelcontextprotocol.io), so any compatible AI
app can query your documents.

| Tool | Description |
|------|-------------|
| `list_documents` | List indexed documents (id, filename, passage count) |
| `search_documents` | Semantic search, optionally restricted to one document |
| `read_document` | Read a document passage by passage (paginated) |
| `add_note` | Save a text note so it becomes searchable |
| `delete_document` | Remove a document or note (marked destructive) |
| `list_conversations` | List past Nexus conversations |
| `get_conversation` | Read the last messages of a conversation |
| `ask_nexus` | Ask the local LLM, optionally grounded in your documents |

### Try the tools from the browser

Open the **MCP** page of the web interface (http://localhost:8501/MCP, or
*MCP* in the sidebar menu). It connects to the MCP server like any external AI
app would, lists the tools, and generates a form for each one from its schema.
It is the easiest way to check that the server works, for example:

1. `add_note` with a title and some content
2. `list_documents` to see the note appear
3. `search_documents` to retrieve it
4. `delete_document` with its `document_id` to clean up

The page connects to `http://127.0.0.1:8001/mcp` by default (override with
`NEXUS_MCP_URL`). It needs Ollama running with the embedding model for
`add_note` and `search_documents`.

> **Security:** by default the API and the MCP server listen on `127.0.0.1` only
> and have no authentication. MCP tools can add and delete documents, so do not
> expose these ports to a network without adding authentication.

### Connect Claude Code (HTTP)

```bash
claude mcp add --transport http nexus http://127.0.0.1:8001/mcp
```

### Connect Claude Desktop (stdio)

Claude Desktop launches the server itself. Add this to `claude_desktop_config.json`
(adjust the path; the `wsl` form is for Claude Desktop on Windows with the project in WSL):

```json
{
  "mcpServers": {
    "nexus": {
      "command": "wsl",
      "args": [
        "bash", "-lc",
        "cd ~/Samy-NASSET/nexus-ai && .venv/bin/python -m app.mcp_server --transport stdio"
      ]
    }
  }
}
```

On native Linux/macOS use `"command": "/path/to/nexus-ai/.venv/bin/python"` with
`"args": ["-m", "app.mcp_server", "--transport", "stdio"]` and set `"cwd"`.

Try asking your MCP client: *"Search my Nexus documents for the deployment
checklist and summarise it."*

## API overview

Interactive documentation: http://localhost:8000/docs

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/health` | Liveness check (with version) |
| GET | `/api/status` | Ollama, models and MCP availability |
| GET / POST | `/api/conversations` | List / create conversations |
| PATCH | `/api/conversations/{id}` | Rename a conversation (`{"title": "..."}`) |
| DELETE | `/api/conversations/{id}` | Delete a conversation |
| GET | `/api/conversations/{id}/messages` | Messages, with the sources of each answer |
| GET | `/api/conversations/{id}/export` | Download the conversation as Markdown |
| GET / POST | `/api/documents` | List / upload documents |
| GET | `/api/documents/{id}` | Read a document's passages |
| DELETE | `/api/documents/{id}` | Delete a document |
| GET | `/api/search?q=...&limit=4` | Semantic search, without calling the chat model |
| POST | `/api/chat` | Chat, single JSON response |
| POST | `/api/chat/stream` | Chat, streamed as NDJSON (`meta`, `token`, `done`, `error`) |

Chat requests accept `{"message": "...", "conversation_id": 1, "use_documents": true}`;
`conversation_id` is optional (a new conversation is created) and
`use_documents: false` skips retrieval.

```bash
curl -s localhost:8000/api/chat -H 'Content-Type: application/json' \
  -d '{"message": "Summarise my deployment notes"}' | python3 -m json.tool
```

## Development

```bash
source .venv/bin/activate
pip install -r requirements-dev.txt    # or: make dev

make lint           # ruff check + ruff format --check
make test           # all tests, no Ollama needed (~20 s)
make cov            # same, with a coverage report (CI fails under 85%)
make smoke          # boots the full stack and checks every service
make smoke-ollama   # same, plus indexing, search, chat and MCP with a real Ollama
make help           # every target
```

| Test file | What it covers |
|-----------|----------------|
| `tests/test_rag.py` | Chunking, text/PDF extraction, embeddings batching, duplicates, search ranking, the shared index between processes |
| `tests/test_storage.py` | Conversations, titles, sources, migrations of older databases, WAL mode |
| `tests/test_llm.py` | Prompt building, history trimming, Ollama errors and streaming |
| `tests/test_api.py` | Every endpoint, including streaming events and error paths |
| `tests/test_mcp.py` | The 8 MCP tools through a real MCP client session |
| `tests/test_ui.py` | Both Streamlit pages driven headlessly against a live API / MCP server |
| `tests/test_config.py` | Environment variable parsing and validation |

Ollama is replaced by deterministic fakes in the tests (bag-of-words
embeddings, scripted chat answers), so the suite is fast and reproducible.

### CI/CD

`.github/workflows/nexus-ai-ci.yml` runs on every push and pull request
touching `nexus-ai/`:

1. **Lint & format**: `ruff check` and `ruff format --check`
2. **Tests** on Python 3.10, 3.11, 3.12 and 3.13, with coverage (report uploaded as an artifact)
3. **Smoke test**: `start.sh` boots API + UI + MCP; the script checks each
   service, a real MCP client and a conversation round trip
4. **Docker**: builds the image, starts a container and checks its health;
   on `main` and on `nexus-ai-v*` tags it **publishes** the image to GHCR

Dependabot (`.github/dependabot.yml`) proposes weekly updates for the Python
packages, the Docker base image and the GitHub Actions.

To release a version: bump `__version__` in `app/__init__.py`, add an entry to
[CHANGELOG.md](CHANGELOG.md), then `git tag nexus-ai-v1.2.0 && git push --tags`.

## Troubleshooting

| Symptom | Fix |
|---------|-----|
| Sidebar shows Ollama 🔴 | Run `ollama serve`, or check `NEXUS_OLLAMA_URL` |
| Model 🟠 / "model not found" | `ollama pull llama3.2:1b` (or your `NEXUS_CHAT_MODEL`) |
| Upload fails with 502 | `ollama pull nomic-embed-text` |
| "No extractable text" | The PDF is a scanned image: OCR it first |
| "Port 8000 is already in use" | Nexus is already running, or change `NEXUS_API_PORT` |
| Answers ignore the documents | Check the *Answer from my documents* switch, then the sources shown under the answer |

## Limitations and ideas

- `llama3.2:1b` is small and fast but often imprecise; use a larger model for quality.
- Possible next steps: OCR for scanned PDFs, hybrid (keyword + vector) search,
  re-ranking, more file types, authentication if exposed beyond localhost.

## License

MIT, like the rest of the repository. See the [LICENSE](LICENSE) file.
