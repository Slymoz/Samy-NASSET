"""Retrieval-Augmented Generation: indexing and semantic search.

Pipeline: file -> text -> overlapping chunks -> embeddings (Ollama) ->
ChromaDB.  Searching embeds the question and returns the closest chunks.

Several processes share the same index (the API and the MCP server). Chroma
keeps its vector index in memory per process, so a document added by one
process is invisible to (or even crashes) searches in the other until the
client is reopened. ``_index_access`` handles this: a file lock serialises
access across processes and a version marker, bumped after every write, tells
each process when to reload its client.
"""

import hashlib
import os
import threading
import uuid
from collections.abc import Iterator
from contextlib import contextmanager
from datetime import datetime, timezone
from pathlib import Path

import chromadb
import httpx
from chromadb.api.shared_system_client import SharedSystemClient
from chromadb.config import Settings
from pypdf import PdfReader
from pypdf.errors import PyPdfError

from app import config

try:  # POSIX only; on other systems we fall back to in-process locking.
    import fcntl
except ImportError:  # pragma: no cover
    fcntl = None

COLLECTION_NAME = "nexus_documents"

_lock = threading.RLock()
_state: dict = {"collection": None, "version": None, "path": None, "depth": 0}


# --------------------------------------------------------------------------- #
# Index access (shared between processes)
# --------------------------------------------------------------------------- #
def _version_file() -> Path:
    return config.CHROMA_DIR / ".version"


def _read_version() -> str:
    try:
        return _version_file().read_text()
    except FileNotFoundError:
        return ""


def _bump_version() -> str:
    version = uuid.uuid4().hex
    tmp = _version_file().with_suffix(".tmp")
    tmp.write_text(version)
    os.replace(tmp, _version_file())  # atomic: readers never see half a file
    return version


def _load_collection():
    """Return the cached collection, reopening it if another process wrote."""
    version = _read_version()
    if (
        _state["collection"] is not None
        and _state["version"] == version
        and _state["path"] == config.CHROMA_DIR
    ):
        return _state["collection"]

    # Chroma caches one client per path: drop it to really reload from disk.
    SharedSystemClient.clear_system_cache()
    client = chromadb.PersistentClient(
        path=str(config.CHROMA_DIR),
        settings=Settings(anonymized_telemetry=False),  # stay 100% local
    )
    _state["collection"] = client.get_or_create_collection(
        name=COLLECTION_NAME,
        metadata={"hnsw:space": "cosine"},
    )
    _state["version"] = version
    _state["path"] = config.CHROMA_DIR
    return _state["collection"]


@contextmanager
def _index_access(write: bool = False) -> Iterator:
    """Yield an up-to-date collection while holding the index lock.

    Reads take a shared lock, writes an exclusive one. Nested calls in the
    same thread reuse the outer lock.
    """
    with _lock:
        if _state["depth"]:  # already locked by this thread
            _state["depth"] += 1
            try:
                yield _load_collection()
            finally:
                _state["depth"] -= 1
            return

        config.ensure_dirs()
        with open(config.CHROMA_DIR / ".lock", "a") as handle:
            if fcntl is not None:
                fcntl.flock(handle.fileno(), fcntl.LOCK_EX if write else fcntl.LOCK_SH)
            _state["depth"] = 1
            try:
                yield _load_collection()
            finally:
                _state["depth"] = 0
                if write:
                    _state["version"] = _bump_version()
                if fcntl is not None:
                    fcntl.flock(handle.fileno(), fcntl.LOCK_UN)


def get_collection():
    """Return the (up-to-date) Chroma collection. Kept for scripts and tests."""
    with _index_access() as collection:
        return collection


# --------------------------------------------------------------------------- #
# Embeddings
# --------------------------------------------------------------------------- #
def embed_texts(texts: list[str]) -> list[list[float]]:
    """Turn texts into vectors with the Ollama embedding model."""
    vectors: list[list[float]] = []

    # Batching avoids huge requests for long documents.
    for i in range(0, len(texts), config.EMBED_BATCH_SIZE):
        batch = texts[i : i + config.EMBED_BATCH_SIZE]
        response = httpx.post(
            f"{config.OLLAMA_URL}/api/embed",
            json={"model": config.EMBED_MODEL, "input": batch},
            timeout=config.LLM_TIMEOUT,
        )
        response.raise_for_status()

        embeddings = response.json().get("embeddings")
        if embeddings is None or len(embeddings) != len(batch):
            raise RuntimeError("Ollama returned invalid embeddings.")
        vectors.extend(embeddings)

    return vectors


# --------------------------------------------------------------------------- #
# Text extraction and chunking
# --------------------------------------------------------------------------- #
def _decode(raw: bytes) -> str:
    """Decode a text file: UTF-8 (with or without BOM), then Windows-1252."""
    for encoding in ("utf-8-sig", "cp1252"):
        try:
            return raw.decode(encoding)
        except UnicodeDecodeError:
            continue
    return raw.decode("latin-1")  # never fails


def extract_text(path: Path) -> str:
    """Extract raw text from a PDF, TXT or Markdown file."""
    if path.suffix.lower() != ".pdf":
        return _decode(path.read_bytes())

    try:
        reader = PdfReader(str(path))
        if reader.is_encrypted and not reader.decrypt(""):
            raise ValueError("This PDF is password-protected.")
        return "\n".join(page.extract_text() or "" for page in reader.pages)
    except (PyPdfError, OSError) as exc:
        raise ValueError(f"Cannot read this PDF: {exc}") from exc


def split_text(text: str, size: int | None = None, overlap: int | None = None) -> list[str]:
    """Split text into chunks of at most ``size`` characters sharing ``overlap``.

    Chunks end on a sentence or word boundary whenever one exists in the
    second half of the window, so passages do not cut words in two. The
    overlap prevents a sentence cut at a boundary from losing its context.
    """
    size = config.CHUNK_SIZE if size is None else size
    overlap = config.CHUNK_OVERLAP if overlap is None else overlap
    if overlap >= size:
        raise ValueError("overlap must be smaller than size")

    text = " ".join(text.split())  # normalise whitespace
    length = len(text)
    chunks: list[str] = []
    start = 0

    while start < length:
        end = min(start + size, length)
        cut = end
        if end < length:
            floor = start + size // 2
            sentence = max(text.rfind(p, floor, end) for p in (". ", "! ", "? "))
            space = text.rfind(" ", floor, end + 1)
            if sentence != -1:
                cut = sentence + 1  # keep the punctuation
            elif space != -1:
                cut = space

        chunk = text[start:cut].strip()
        if chunk:
            chunks.append(chunk)
        if cut >= length:
            break

        next_start = max(cut - overlap, start + 1)
        if text[next_start - 1] != " ":  # landed inside a word: skip to its end
            space = text.find(" ", next_start, cut)
            if space != -1:
                next_start = space + 1
        start = next_start

    return chunks


# --------------------------------------------------------------------------- #
# Indexing
# --------------------------------------------------------------------------- #
def _find_by_hash(digest: str) -> dict | None:
    with _index_access() as collection:
        data = collection.get(where={"sha256": digest}, limit=1, include=["metadatas"])
    if not data["ids"]:
        return None
    metadata = data["metadatas"][0]
    return {"document_id": metadata["document_id"], "filename": metadata["filename"]}


def _count_chunks(document_id: str) -> int:
    with _index_access() as collection:
        return len(collection.get(where={"document_id": document_id}, include=[])["ids"])


def index_file(filename: str, content: bytes) -> dict:
    """Store a file, split it, embed it and add it to the index.

    Uploading the exact same content twice returns the existing document
    (with ``duplicate: True``) instead of indexing it again.
    """
    safe_name = Path(filename).name  # strip any directory component
    if Path(safe_name).suffix.lower() not in config.ALLOWED_EXTENSIONS:
        raise ValueError("Supported formats: PDF, TXT and Markdown.")
    if not content:
        raise ValueError("The file is empty.")

    digest = hashlib.sha256(content).hexdigest()
    existing = _find_by_hash(digest)
    if existing:
        return {
            **existing,
            "chunks": _count_chunks(existing["document_id"]),
            "duplicate": True,
        }

    config.ensure_dirs()
    document_id = str(uuid.uuid4())
    saved_path = config.UPLOAD_DIR / f"{document_id}_{safe_name}"
    saved_path.write_bytes(content)

    try:
        chunks = split_text(extract_text(saved_path))
        if not chunks:
            raise ValueError("No extractable text. The PDF may be a scanned image.")

        embeddings = embed_texts(chunks)  # slow network call: outside the lock
        indexed_at = datetime.now(timezone.utc).isoformat(timespec="seconds")
        with _index_access(write=True) as collection:
            collection.add(
                ids=[f"{document_id}_{i}" for i in range(len(chunks))],
                documents=chunks,
                embeddings=embeddings,
                metadatas=[
                    {
                        "document_id": document_id,
                        "filename": safe_name,
                        "chunk": i,
                        "sha256": digest,
                        "indexed_at": indexed_at,
                    }
                    for i in range(len(chunks))
                ],
            )
    except Exception:
        saved_path.unlink(missing_ok=True)  # do not keep half-indexed files
        raise

    return {
        "document_id": document_id,
        "filename": safe_name,
        "chunks": len(chunks),
        "duplicate": False,
    }


def index_text(title: str, text: str) -> dict:
    """Index a piece of text (e.g. a note) as a Markdown document."""
    name = Path(title.strip() or "note").name or "note"
    if Path(name).suffix.lower() not in config.ALLOWED_EXTENSIONS:
        name += ".md"
    return index_file(name, text.encode("utf-8"))


# --------------------------------------------------------------------------- #
# Reading and deleting
# --------------------------------------------------------------------------- #
def _is_uuid(value: str) -> bool:
    try:
        uuid.UUID(value)
    except (ValueError, TypeError, AttributeError):
        return False
    return True


def list_documents() -> list[dict]:
    """List indexed documents with their number of chunks."""
    with _index_access() as collection:
        data = collection.get(include=["metadatas"])
    documents: dict[str, dict] = {}

    for metadata in data.get("metadatas") or []:
        if not metadata:
            continue
        entry = documents.setdefault(
            metadata["document_id"],
            {
                "document_id": metadata["document_id"],
                "filename": metadata["filename"],
                "chunks": 0,
                "indexed_at": metadata.get("indexed_at"),
            },
        )
        entry["chunks"] += 1

    return sorted(documents.values(), key=lambda d: (d["filename"].lower(), d["document_id"]))


def get_document_chunks(document_id: str) -> list[dict]:
    """Return every chunk of a document, in reading order."""
    if not _is_uuid(document_id):
        return []
    with _index_access() as collection:
        data = collection.get(
            where={"document_id": document_id},
            include=["documents", "metadatas"],
        )
    chunks = [
        {
            "filename": metadata["filename"],
            "chunk": metadata["chunk"],
            "text": text,
        }
        for text, metadata in zip(
            data.get("documents") or [], data.get("metadatas") or [], strict=True
        )
    ]
    return sorted(chunks, key=lambda c: c["chunk"])


def delete_document(document_id: str) -> bool:
    """Remove a document from the index and from disk.

    Returns False when the document is unknown.
    """
    if not _is_uuid(document_id):  # also blocks glob patterns in the id
        return False

    with _index_access(write=True) as collection:
        existing = collection.get(where={"document_id": document_id}, include=[])
        if not existing["ids"]:
            return False
        collection.delete(where={"document_id": document_id})

    for path in config.UPLOAD_DIR.glob(f"{document_id}_*"):
        path.unlink(missing_ok=True)
    return True


# --------------------------------------------------------------------------- #
# Search
# --------------------------------------------------------------------------- #
def search_documents(
    query: str, limit: int | None = None, document_id: str | None = None
) -> list[dict]:
    """Return the chunks closest to ``query`` (lower distance = closer).

    ``document_id`` restricts the search to a single document. Passages
    farther than ``NEXUS_MAX_DISTANCE`` (when set) are dropped.
    """
    query = query.strip()
    if not query or (document_id and not _is_uuid(document_id)):
        return []

    with _index_access() as collection:
        total = collection.count()
    if total == 0:
        return []

    limit = config.TOP_K if limit is None else limit
    limit = max(1, min(int(limit), config.MAX_TOP_K))
    vector = embed_texts([query])[0]  # slow network call: outside the lock

    with _index_access() as collection:
        result = collection.query(
            query_embeddings=[vector],
            n_results=min(limit, total),
            where={"document_id": document_id} if document_id else None,
            include=["documents", "metadatas", "distances"],
        )

    passages = [
        {
            "document_id": metadata["document_id"],
            "filename": metadata["filename"],
            "chunk": metadata["chunk"],
            "text": text,
            "distance": float(distance),
        }
        for text, metadata, distance in zip(
            result["documents"][0],
            result["metadatas"][0],
            result["distances"][0],
            strict=True,
        )
    ]
    if config.MAX_DISTANCE is not None:
        passages = [p for p in passages if p["distance"] <= config.MAX_DISTANCE]
    return passages
