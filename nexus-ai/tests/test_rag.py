import io
import subprocess
import sys
import textwrap
from pathlib import Path

import httpx
import pytest
from pypdf import PdfWriter

from app import config, rag

ROOT = Path(__file__).resolve().parent.parent


# --------------------------------------------------------------------------- #
# Chunking
# --------------------------------------------------------------------------- #
def test_split_text_overlaps_and_covers_everything():
    text = "abcdefghij" * 30  # 300 chars, no space: hard cuts
    chunks = rag.split_text(text, size=100, overlap=20)

    assert all(len(c) <= 100 for c in chunks)
    assert chunks[0][-20:] == chunks[1][:20]  # consecutive chunks overlap
    assert chunks[-1].endswith(text[-10:])  # nothing is lost at the end


def test_split_text_handles_empty_and_short_input():
    assert rag.split_text("   ") == []
    assert rag.split_text("hello world") == ["hello world"]


def test_split_text_rejects_bad_overlap():
    with pytest.raises(ValueError):
        rag.split_text("text", size=10, overlap=10)


def test_split_text_never_cuts_words():
    words = [f"word{i:03d}" for i in range(300)]
    chunks = rag.split_text(" ".join(words), size=100, overlap=25)

    assert all(len(c) <= 100 for c in chunks)
    vocabulary = set(words)
    for chunk in chunks:
        assert set(chunk.split()) <= vocabulary  # every token is a whole word
    covered = {w for c in chunks for w in c.split()}
    assert covered == vocabulary  # and every word is somewhere
    assert chunks[0].split()[-1] in chunks[1]  # neighbours still overlap


def test_split_text_prefers_sentence_boundaries():
    text = "First sentence is here. " * 3 + "Second part keeps going without a stop " * 3
    chunks = rag.split_text(text, size=90, overlap=10)
    assert chunks[0].endswith(".")


def test_split_text_uses_configured_defaults(monkeypatch):
    monkeypatch.setattr(config, "CHUNK_SIZE", 50)
    monkeypatch.setattr(config, "CHUNK_OVERLAP", 5)
    assert all(len(c) <= 50 for c in rag.split_text("lorem ipsum " * 40))


# --------------------------------------------------------------------------- #
# Text extraction
# --------------------------------------------------------------------------- #
def test_extract_text_decodes_utf8_bom_and_windows_1252(tmp_path):
    utf8 = tmp_path / "a.txt"
    utf8.write_bytes("﻿Café crème".encode())
    legacy = tmp_path / "b.txt"
    legacy.write_bytes("Café – déjà".encode("cp1252"))

    assert rag.extract_text(utf8) == "Café crème"
    assert rag.extract_text(legacy) == "Café – déjà"


def _pdf_bytes(password: str | None = None) -> bytes:
    writer = PdfWriter()
    writer.add_blank_page(width=200, height=200)
    if password:
        writer.encrypt(password)
    buffer = io.BytesIO()
    writer.write(buffer)
    return buffer.getvalue()


def test_pdf_without_text_is_rejected_and_not_kept(fake_embeddings):
    with pytest.raises(ValueError, match="No extractable text"):
        rag.index_file("scan.pdf", _pdf_bytes())
    assert list(config.UPLOAD_DIR.iterdir()) == []  # file cleaned up


def test_encrypted_pdf_gives_a_clear_error(fake_embeddings):
    with pytest.raises(ValueError, match="password"):
        rag.index_file("secret.pdf", _pdf_bytes(password="s3cret"))


def test_corrupt_pdf_gives_a_clear_error(fake_embeddings):
    with pytest.raises(ValueError, match="Cannot read this PDF"):
        rag.index_file("broken.pdf", b"%PDF-1.4 this is not really a pdf")


# --------------------------------------------------------------------------- #
# Embeddings
# --------------------------------------------------------------------------- #
def test_embed_texts_batches_requests(monkeypatch):
    monkeypatch.setattr(config, "EMBED_BATCH_SIZE", 2)
    batches = []

    def fake_post(url, json, timeout):
        batches.append(json["input"])
        request = httpx.Request("POST", url)
        return httpx.Response(
            200, json={"embeddings": [[1.0, 0.0]] * len(json["input"])}, request=request
        )

    monkeypatch.setattr(rag.httpx, "post", fake_post)
    assert len(rag.embed_texts(["a", "b", "c", "d", "e"])) == 5
    assert [len(b) for b in batches] == [2, 2, 1]


def test_embed_texts_rejects_invalid_answers(monkeypatch):
    def fake_post(url, json, timeout):
        return httpx.Response(200, json={"embeddings": []}, request=httpx.Request("POST", url))

    monkeypatch.setattr(rag.httpx, "post", fake_post)
    with pytest.raises(RuntimeError):
        rag.embed_texts(["a"])


# --------------------------------------------------------------------------- #
# Index, search, read, delete
# --------------------------------------------------------------------------- #
def test_index_search_read_delete_roundtrip(fake_embeddings):
    info = rag.index_text("my note", "Nexus stores notes in ChromaDB.")
    doc_id = info["document_id"]
    assert info["filename"] == "my note.md"
    assert info["duplicate"] is False

    listed = rag.list_documents()
    assert [d["document_id"] for d in listed] == [doc_id]
    assert listed[0]["indexed_at"]
    assert rag.search_documents("chroma", document_id=doc_id)[0]["filename"] == "my note.md"
    assert rag.get_document_chunks(doc_id)[0]["text"].startswith("Nexus stores")

    assert rag.delete_document(doc_id) is True
    assert rag.delete_document(doc_id) is False
    assert rag.delete_document("../../etc/passwd") is False
    assert rag.list_documents() == []
    assert list(config.UPLOAD_DIR.iterdir()) == []


def test_unsupported_extension_is_rejected():
    with pytest.raises(ValueError):
        rag.index_file("virus.exe", b"data")


def test_empty_file_is_rejected():
    with pytest.raises(ValueError, match="empty"):
        rag.index_file("empty.txt", b"")


def test_filename_directory_components_are_stripped(fake_embeddings):
    info = rag.index_file("../../outside.txt", b"some content")
    assert info["filename"] == "outside.txt"
    assert all(p.parent == config.UPLOAD_DIR for p in config.UPLOAD_DIR.iterdir())


def test_duplicate_content_is_not_indexed_twice(fake_embeddings):
    first = rag.index_file("a.txt", b"exactly the same content")
    second = rag.index_file("b.txt", b"exactly the same content")

    assert second["duplicate"] is True
    assert second["document_id"] == first["document_id"]
    assert second["filename"] == "a.txt"
    assert second["chunks"] == first["chunks"]
    assert len(rag.list_documents()) == 1


def test_search_ranks_the_relevant_document_first(fake_embeddings):
    rag.index_text("cooking", "pasta tomato basil olive oil recipe")
    rag.index_text("deploy", "kubernetes deployment checklist rollback canary")

    results = rag.search_documents("deployment checklist")
    assert results[0]["filename"] == "deploy.md"
    assert results[0]["distance"] < results[1]["distance"]


def test_search_limits_and_threshold(fake_embeddings, monkeypatch):
    for i in range(12):
        rag.index_text(f"note{i}", f"shared topic number{i}")

    assert len(rag.search_documents("shared topic")) == config.TOP_K
    assert len(rag.search_documents("shared topic", limit=50)) == config.MAX_TOP_K
    assert len(rag.search_documents("shared topic", limit=0)) == 1

    monkeypatch.setattr(config, "MAX_DISTANCE", 0.0)
    assert rag.search_documents("completely unrelated words") == []


def test_search_edge_cases(fake_embeddings):
    assert rag.search_documents("anything") == []  # empty index: no Ollama call
    assert fake_embeddings == []
    rag.index_text("n", "content")
    assert rag.search_documents("   ") == []
    assert rag.search_documents("content", document_id="not-a-uuid") == []
    assert rag.get_document_chunks("not-a-uuid") == []


def test_index_is_shared_between_processes(fake_embeddings):
    """A document added by another process (e.g. the MCP server) is found.

    Regression test: Chroma keeps its vector index in memory per process, so
    without the version marker, searches here either missed the new document
    or crashed with "Error creating hnsw segment reader".
    """
    assert rag.search_documents("anything") == []  # load the (empty) index here

    script = textwrap.dedent(
        """
        import sys
        from app import config, rag
        from pathlib import Path
        from tests.conftest import fake_embedding
        root = Path(sys.argv[1])
        config.CHROMA_DIR, config.UPLOAD_DIR = root / "chroma", root / "documents"
        rag.embed_texts = lambda texts: [fake_embedding(t) for t in texts]
        rag.index_text("remote", "written by another process")
        """
    )
    subprocess.run(
        [sys.executable, "-c", script, str(config.DATA_DIR)], cwd=ROOT, check=True, timeout=120
    )

    results = rag.search_documents("another process")
    assert [r["filename"] for r in results] == ["remote.md"]
    assert [d["filename"] for d in rag.list_documents()] == ["remote.md"]
