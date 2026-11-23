# Changelog

All notable changes to Nexus AI. Versions follow [Semantic Versioning](https://semver.org).

## [1.1.0] - 2026-11-23

### Fixed

- **Shared document index between the API and the MCP server.** Each process
  kept its own in-memory copy of the Chroma vector index, so a note added
  through MCP was missing from API searches (or crashed them with
  `Error creating hnsw segment reader`) until a restart, and vice versa. Access
  is now guarded by a file lock and a version marker that makes each process
  reload the index after the other one writes.
- `requirements.txt` allowed `mcp` versions without the streamable HTTP client
  the project uses; it now requires `mcp>=1.24`.
- Streaming chat could hang forever if Ollama stalled (`timeout=None`); the
  timeout now applies between two tokens (`NEXUS_LLM_TIMEOUT`).
- Long conversations were sent to the model in full and could overflow its
  context; only the last `NEXUS_HISTORY_MESSAGES` messages are sent.
- Uploads were read entirely in memory before the size check.
- Encrypted or corrupt PDFs returned a generic 500 error; they now return a
  clear 400 message. Windows-1252 text files are no longer garbled.
- Ollama errors (e.g. unknown model) are reported with Ollama's own message.

### Added

- Sources are saved with each answer and shown again in the history.
- Rename a conversation (`PATCH /api/conversations/{id}`) and export it as
  Markdown (`GET /api/conversations/{id}/export`).
- `GET /api/search` and `GET /api/documents/{id}` endpoints.
- `use_documents` chat option and an *Answer from my documents* switch in the UI.
- Multiple file upload in the UI.
- Duplicate detection: the same content is indexed only once.
- Passages are cut on sentence or word boundaries.
- Settings for chunking, top-k, distance threshold, temperature, timeouts and
  upload limit, validated at startup; `.env` support in `start.sh`.
- MCP tool annotations (read-only / destructive hints).
- SQLite WAL mode and an automatic migration for existing databases.
- Dockerfile and `docker-compose.yml` (Ollama + model download + Nexus).
- Makefile, `.env.example`, Dependabot configuration.
- Test suite grown from 9 to ~80 tests: API, real MCP client sessions,
  Streamlit UI tests (AppTest), migrations, cross-process index; 90% coverage.
- CI/CD: ruff lint and format check, test matrix on Python 3.10 to 3.13 with
  coverage, smoke test, Docker build and container check, image publication
  to GHCR on `main` and `nexus-ai-v*` tags.

### Changed

- Chroma anonymous telemetry is disabled (the project is meant to stay 100% local).
- Streamlit buttons use `width="stretch"` (`use_container_width` is deprecated);
  requires `streamlit>=1.50`.
- `start.sh` checks that its ports are free and accepts `NEXUS_HOST` (used by Docker).

## [1.0.0]

- First version: streaming chat with Ollama, RAG over PDF/TXT/Markdown with
  ChromaDB, SQLite history, MCP server with 8 tools, MCP playground page.
