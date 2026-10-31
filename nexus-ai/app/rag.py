"""Retrieval-Augmented Generation: indexing and semantic search.

Pipeline: file -> text -> overlapping chunks -> embeddings (Ollama) ->
ChromaDB.  Searching embeds the question and returns the closest chunks.
"""

import uuid
from functools import lru_cache
from pathlib import Path

import chromadb
import httpx
from pypdf import PdfReader

from app import config


@lru_cache(maxsize=1)
def get_collection():
    """Open the persistent Chroma collection on first use.

    Doing this lazily (not at import time) keeps imports cheap and lets tests
    point ``NEXUS_DATA_DIR`` somewhere else before anything touches the disk.
    """
    config.ensure_dirs()
    client = chromadb.PersistentClient(path=str(config.CHROMA_DIR))
    return client.get_or_create_collection(
        name="nexus_documents",
        metadata={"hnsw:space": "cosine"},
    )


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
            timeout=180,
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
def extract_text(path: Path) -> str:
    """Extract raw text from a PDF, TXT or Markdown file."""
    if path.suffix.lower() == ".pdf":
        reader = PdfReader(str(path))
        return "\n".join(page.extract_text() or "" for page in reader.pages)
    return path.read_text(encoding="utf-8", errors="replace")


def split_text(
    text: str,
    size: int = config.CHUNK_SIZE,
    overlap: int = config.CHUNK_OVERLAP,
) -> list[str]:
    """Split text into chunks of ``size`` characters sharing ``overlap``.

    The overlap prevents a sentence cut at a chunk boundary from losing its
    context.
    """
    if overlap >= size:
        raise ValueError("overlap must be smaller than size")

    text = " ".join(text.split())  # normalise whitespace
    chunks: list[str] = []
    start = 0

    while start < len(text):
        end = min(start + size, len(text))
        chunks.append(text[start:end])
        if end == len(text):
            break
        start = end - overlap

    return chunks


# --------------------------------------------------------------------------- #
# Indexing
# --------------------------------------------------------------------------- #
def index_file(filename: str, content: bytes) -> dict:
    """Store a file, split it, embed it and add it to the index."""
    safe_name = Path(filename).name  # strip any directory component
    if Path(safe_name).suffix.lower() not in config.ALLOWED_EXTENSIONS:
        raise ValueError("Supported formats: PDF, TXT and Markdown.")
    if not content:
        raise ValueError("The file is empty.")

    config.ensure_dirs()
    document_id = str(uuid.uuid4())
    saved_path = config.UPLOAD_DIR / f"{document_id}_{safe_name}"
    saved_path.write_bytes(content)

    try:
        chunks = split_text(extract_text(saved_path))
        if not chunks:
            raise ValueError(
                "No extractable text. The PDF may be a scanned image."
            )

        get_collection().add(
            ids=[f"{document_id}_{i}" for i in range(len(chunks))],
            documents=chunks,
            embeddings=embed_texts(chunks),
            metadatas=[
                {"document_id": document_id, "filename": safe_name, "chunk": i}
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
    }


def index_text(title: str, text: str) -> dict:
    """Index a piece of text (e.g. a note) as a Markdown document."""
    name = Path(title.strip() or "note").name
    if Path(name).suffix.lower() not in config.ALLOWED_EXTENSIONS:
        name += ".md"
    return index_file(name, text.encode("utf-8"))


# --------------------------------------------------------------------------- #
# Reading and deleting
# --------------------------------------------------------------------------- #
def list_documents() -> list[dict]:
    """List indexed documents with their number of chunks."""
    data = get_collection().get(include=["metadatas"])
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
            },
        )
        entry["chunks"] += 1

    return sorted(
        documents.values(), key=lambda d: (d["filename"], d["document_id"])
    )


def get_document_chunks(document_id: str) -> list[dict]:
    """Return every chunk of a document, in reading order."""
    data = get_collection().get(
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
            data.get("documents") or [], data.get("metadatas") or []
        )
    ]
    return sorted(chunks, key=lambda c: c["chunk"])


def delete_document(document_id: str) -> bool:
    """Remove a document from the index and from disk.

    Returns False when the document is unknown.
    """
    try:
        uuid.UUID(document_id)  # also blocks glob patterns in the id
    except ValueError:
        return False

    collection = get_collection()
    existing = collection.get(where={"document_id": document_id})
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
    query: str, limit: int = 4, document_id: str | None = None
) -> list[dict]:
    """Return the chunks closest to ``query`` (lower distance = closer).

    ``document_id`` restricts the search to a single document.
    """
    query = query.strip()
    collection = get_collection()
    total = collection.count()
    if not query or total == 0:
        return []

    limit = max(1, min(int(limit), 10))
    result = collection.query(
        query_embeddings=[embed_texts([query])[0]],
        n_results=min(limit, total),
        where={"document_id": document_id} if document_id else None,
        include=["documents", "metadatas", "distances"],
    )

    return [
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
        )
    ]
