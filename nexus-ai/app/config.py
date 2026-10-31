"""Central configuration.

Every value can be overridden with an environment variable, which keeps the
code free of hard-coded paths and makes the project easy to run in CI.
"""

import os
from pathlib import Path

ROOT_DIR = Path(__file__).resolve().parent.parent

# --- Storage -----------------------------------------------------------------
DATA_DIR = Path(os.getenv("NEXUS_DATA_DIR", ROOT_DIR / "data"))
UPLOAD_DIR = DATA_DIR / "documents"  # original uploaded files
CHROMA_DIR = DATA_DIR / "chroma"  # vector index
DB_PATH = DATA_DIR / "nexus.db"  # SQLite conversation history

# --- Ollama ------------------------------------------------------------------
OLLAMA_URL = os.getenv("NEXUS_OLLAMA_URL", "http://127.0.0.1:11434")
CHAT_MODEL = os.getenv("NEXUS_CHAT_MODEL", "llama3.2:1b")
EMBED_MODEL = os.getenv("NEXUS_EMBED_MODEL", "nomic-embed-text")

# --- MCP server --------------------------------------------------------------
MCP_HOST = os.getenv("NEXUS_MCP_HOST", "127.0.0.1")
MCP_PORT = int(os.getenv("NEXUS_MCP_PORT", "8001"))

# --- RAG ---------------------------------------------------------------------
CHUNK_SIZE = 900  # characters per passage
CHUNK_OVERLAP = 150  # characters shared between consecutive passages
EMBED_BATCH_SIZE = 32  # passages sent to Ollama per embedding request
ALLOWED_EXTENSIONS = {".pdf", ".txt", ".md"}
MAX_UPLOAD_BYTES = 25 * 1024 * 1024


def ensure_dirs() -> None:
    """Create the data directories if they do not exist yet."""
    UPLOAD_DIR.mkdir(parents=True, exist_ok=True)
    CHROMA_DIR.mkdir(parents=True, exist_ok=True)
