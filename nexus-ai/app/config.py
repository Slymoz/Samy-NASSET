"""Central configuration.

Every value can be overridden with an environment variable, which keeps the
code free of hard-coded paths and makes the project easy to run in CI or in a
container.
"""

import os
from pathlib import Path

ROOT_DIR = Path(__file__).resolve().parent.parent


def _int(name: str, default: int, minimum: int = 0) -> int:
    """Read an integer setting, failing with a clear message if it is invalid."""
    raw = os.getenv(name)
    if raw is None or raw.strip() == "":
        return default
    try:
        value = int(raw)
    except ValueError:
        raise ValueError(f"{name} must be an integer, got {raw!r}") from None
    if value < minimum:
        raise ValueError(f"{name} must be >= {minimum}, got {value}")
    return value


def _float(name: str, default: float | None) -> float | None:
    raw = os.getenv(name)
    if raw is None or raw.strip() == "":
        return default
    try:
        return float(raw)
    except ValueError:
        raise ValueError(f"{name} must be a number, got {raw!r}") from None


# --- Storage -----------------------------------------------------------------
DATA_DIR = Path(os.getenv("NEXUS_DATA_DIR", ROOT_DIR / "data"))
UPLOAD_DIR = DATA_DIR / "documents"  # original uploaded files
CHROMA_DIR = DATA_DIR / "chroma"  # vector index
DB_PATH = DATA_DIR / "nexus.db"  # SQLite conversation history

# --- Ollama ------------------------------------------------------------------
OLLAMA_URL = os.getenv("NEXUS_OLLAMA_URL", "http://127.0.0.1:11434").rstrip("/")
CHAT_MODEL = os.getenv("NEXUS_CHAT_MODEL", "llama3.2:1b")
EMBED_MODEL = os.getenv("NEXUS_EMBED_MODEL", "nomic-embed-text")
LLM_TIMEOUT = _int("NEXUS_LLM_TIMEOUT", 300, minimum=1)  # seconds without a token
TEMPERATURE = _float("NEXUS_TEMPERATURE", None)  # None = model default

# --- Conversation ------------------------------------------------------------
# Only the most recent messages are sent back to the model, so long
# conversations never overflow the model's context window.
HISTORY_MESSAGES = _int("NEXUS_HISTORY_MESSAGES", 20)

# --- MCP server --------------------------------------------------------------
MCP_HOST = os.getenv("NEXUS_MCP_HOST", "127.0.0.1")
MCP_PORT = _int("NEXUS_MCP_PORT", 8001, minimum=1)

# --- RAG ---------------------------------------------------------------------
CHUNK_SIZE = _int("NEXUS_CHUNK_SIZE", 900, minimum=50)  # characters per passage
CHUNK_OVERLAP = _int("NEXUS_CHUNK_OVERLAP", 150)  # characters shared by neighbours
if CHUNK_OVERLAP >= CHUNK_SIZE:
    raise ValueError("NEXUS_CHUNK_OVERLAP must be smaller than NEXUS_CHUNK_SIZE")
TOP_K = _int("NEXUS_TOP_K", 4, minimum=1)  # passages retrieved per question
MAX_TOP_K = 10
# Passages farther than this cosine distance are ignored (None = keep all).
MAX_DISTANCE = _float("NEXUS_MAX_DISTANCE", None)
EMBED_BATCH_SIZE = _int("NEXUS_EMBED_BATCH_SIZE", 32, minimum=1)
ALLOWED_EXTENSIONS = {".pdf", ".txt", ".md"}
MAX_UPLOAD_BYTES = _int("NEXUS_MAX_UPLOAD_MB", 25, minimum=1) * 1024 * 1024


def ensure_dirs() -> None:
    """Create the data directories if they do not exist yet."""
    UPLOAD_DIR.mkdir(parents=True, exist_ok=True)
    CHROMA_DIR.mkdir(parents=True, exist_ok=True)
