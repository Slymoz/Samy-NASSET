import pytest

from app import rag


def test_split_text_overlaps_and_covers_everything():
    text = "abcdefghij" * 30  # 300 chars
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


def test_index_search_read_delete_roundtrip(monkeypatch):
    # Replace the Ollama call with fixed vectors: we test our pipeline, not Ollama.
    monkeypatch.setattr(
        rag, "embed_texts", lambda texts: [[1.0, 0.0, 0.0] for _ in texts]
    )

    info = rag.index_text("my note", "Nexus stores notes in ChromaDB.")
    doc_id = info["document_id"]
    assert info["filename"] == "my note.md"

    assert any(d["document_id"] == doc_id for d in rag.list_documents())
    assert rag.search_documents("chroma", document_id=doc_id)[0]["filename"] == "my note.md"
    assert rag.get_document_chunks(doc_id)[0]["text"].startswith("Nexus stores")

    assert rag.delete_document(doc_id) is True
    assert rag.delete_document(doc_id) is False
    assert rag.delete_document("../../etc/passwd") is False


def test_unsupported_extension_is_rejected():
    with pytest.raises(ValueError):
        rag.index_file("virus.exe", b"data")
