# ✳ Nexus AI

![Nexus AI CI](https://github.com/Slymoz/Samy-NASSET/actions/workflows/nexus-ai-ci.yml/badge.svg)

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
- **RAG**: import PDF / TXT / Markdown, retrieve the most relevant passages,
  show the sources used for each answer
- **Conversation history** persisted in SQLite
- **MCP server** exposing 8 tools (search, read, add notes, browse history, ask the local LLM)
- **MCP playground**: a web page to try every MCP tool from the browser, no command line needed
- **Tested**: unit tests plus a CI smoke test that boots the whole stack
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
| `app/main.py` | REST API: chat, streaming, conversations, documents, status |
| `app/rag.py` | Text extraction, chunking, embeddings, ChromaDB search |
| `app/llm.py` | Prompt building and Ollama chat client |
| `app/storage.py` | SQLite conversation history |
| `app/mcp_server.py` | MCP tools for external AI apps |
| `frontend/streamlit_app.py` | Web UI |
| `frontend/pages/1_MCP.py` | Web page to test the MCP tools (a small MCP client) |
| `start.sh` | Starts API + UI + MCP and stops them together |

## Quick start

These steps assume **Linux or WSL (Ubuntu)**. On Windows, run everything inside
WSL. Python projects are run from a **virtual environment** (`venv`) so
dependencies stay isolated from the system Python.

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
pip install -r requirements.txt
```

### 3. Start everything

```bash
./start.sh
```

| Service | URL |
|---------|-----|
| Web interface | http://localhost:8501 |
| MCP playground | http://localhost:8501/MCP |
| API (Swagger docs) | http://localhost:8000/docs |
| MCP server | http://127.0.0.1:8001/mcp |

Press `Ctrl+C` to stop all services. The sidebar shows the status of each one.
On WSL2, open the URLs from your Windows browser (`localhost` is forwarded).

## Configuration

Environment variables (all optional):

| Variable | Default | Purpose |
|----------|---------|---------|
| `NEXUS_CHAT_MODEL` | `llama3.2:1b` | Ollama chat model |
| `NEXUS_EMBED_MODEL` | `nomic-embed-text` | Ollama embedding model |
| `NEXUS_OLLAMA_URL` | `http://127.0.0.1:11434` | Ollama address |
| `NEXUS_DATA_DIR` | `./data` | Documents, vector index and SQLite file |
| `NEXUS_API_PORT` / `NEXUS_UI_PORT` / `NEXUS_MCP_PORT` | `8000` / `8501` / `8001` | Service ports |
| `NEXUS_MCP_URL` | `http://127.0.0.1:8001/mcp` | MCP server address used by the MCP page |

Example with a larger model:

```bash
ollama pull llama3.1:8b
NEXUS_CHAT_MODEL=llama3.1:8b ./start.sh
```

## How RAG works here

1. **Index**: the file is saved, its text extracted, split into 900-character
   passages with a 150-character overlap, embedded with `nomic-embed-text`
   and stored in ChromaDB (cosine distance).
2. **Retrieve**: the question is embedded and the 4 closest passages are fetched.
3. **Generate**: the passages are injected into the prompt for that turn only,
   and the model is told to cite file names and not to invent sources.

The UI lists the retrieved passages with their distance (lower is closer).

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
| `delete_document` | Remove a document or note |
| `list_conversations` | List past Nexus conversations |
| `get_conversation` | Read the messages of a conversation |
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

> **Security:** the API and the MCP server listen on `127.0.0.1` only and have no
> authentication. MCP tools can add and delete documents, so do not expose these
> ports to a network without adding authentication.

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

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/health` | Liveness check |
| GET | `/api/status` | Ollama, models and MCP availability |
| GET / POST | `/api/conversations` | List / create conversations |
| DELETE | `/api/conversations/{id}` | Delete a conversation |
| GET | `/api/conversations/{id}/messages` | Conversation messages |
| GET / POST | `/api/documents` | List / upload documents |
| DELETE | `/api/documents/{id}` | Delete a document |
| POST | `/api/chat` | Chat, single JSON response |
| POST | `/api/chat/stream` | Chat, streamed as NDJSON (`meta`, `token`, `done`, `error`) |

## Development

```bash
source .venv/bin/activate
pip install -r requirements-dev.txt

ruff check .                # lint
pytest                      # unit tests (no Ollama needed)
./scripts/smoke_test.sh     # boots the full stack and checks every service
```

The GitHub Actions workflow (`.github/workflows/nexus-ai-ci.yml`) runs these
three steps on every push and pull request touching `nexus-ai/`. The smoke test
does not need Ollama: it checks that the API, Streamlit, ChromaDB, SQLite and the MCP
server (via a real MCP client listing and calling tools) all start correctly.

## Limitations and ideas

- `llama3.2:1b` is small and fast but often imprecise; use a larger model for quality.
- Possible next steps: OCR for scanned PDFs, hybrid (keyword + vector) search,
  re-ranking, more file types, authentication if exposed beyond localhost.

## License

MIT, like the rest of the repository.
