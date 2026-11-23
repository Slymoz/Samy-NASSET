"""Shared test setup.

* ``NEXUS_DATA_DIR`` points to a throw-away directory *before* ``app.config``
  is imported, so nothing can ever touch real data.
* Every test then gets its own empty data directory (SQLite + Chroma).
* Ollama is never called: embeddings and the chat model are replaced by
  deterministic fakes.
"""

import hashlib
import math
import os
import tempfile

os.environ["NEXUS_DATA_DIR"] = tempfile.mkdtemp(prefix="nexus-test-")

import pytest
from fastapi.testclient import TestClient

from app import config, llm, rag, storage
from app.main import app as api_app

EMBED_DIM = 64


def fake_embedding(text: str) -> list[float]:
    """Bag-of-words vector: texts sharing words are close, others are not."""
    vector = [0.0] * EMBED_DIM
    for word in text.lower().split():
        word = word.strip(".,;:!?\"'()")
        if word:
            vector[int(hashlib.md5(word.encode()).hexdigest(), 16) % EMBED_DIM] += 1.0
    norm = math.sqrt(sum(v * v for v in vector)) or 1.0
    return [v / norm for v in vector] if any(vector) else [1.0] + [0.0] * (EMBED_DIM - 1)


@pytest.fixture(autouse=True)
def isolated_data(tmp_path, monkeypatch):
    """Give each test a fresh, empty data directory."""
    monkeypatch.setattr(config, "DATA_DIR", tmp_path)
    monkeypatch.setattr(config, "UPLOAD_DIR", tmp_path / "documents")
    monkeypatch.setattr(config, "CHROMA_DIR", tmp_path / "chroma")
    monkeypatch.setattr(config, "DB_PATH", tmp_path / "nexus.db")
    storage.init_db()
    yield tmp_path


@pytest.fixture
def fake_embeddings(monkeypatch):
    calls: list[list[str]] = []

    def embed(texts):
        calls.append(list(texts))
        return [fake_embedding(t) for t in texts]

    monkeypatch.setattr(rag, "embed_texts", embed)
    return calls


class FakeLLM:
    """Stands in for Ollama's chat endpoint and records what it receives."""

    def __init__(self, tokens=("Hello", " from", " Nexus"), error=None):
        self.tokens = list(tokens)
        self.error = error
        self.calls: list[list[dict]] = []

    async def stream_chat(self, messages):
        self.calls.append(messages)
        for token in self.tokens:
            yield {"message": {"content": token}, "done": False}
        if self.error:
            raise self.error
        yield {"message": {"content": ""}, "done": True, "eval_count": 3, "eval_duration": 1.5e9}

    def generate_answer(self, question, history=(), sources=()):
        self.calls.append(llm.build_messages(question, history, sources))
        if self.error:
            raise self.error
        return "".join(self.tokens)


@pytest.fixture
def fake_llm(monkeypatch):
    fake = FakeLLM()
    monkeypatch.setattr(llm, "stream_chat", fake.stream_chat)
    monkeypatch.setattr(llm, "generate_answer", fake.generate_answer)
    return fake


@pytest.fixture
def client():
    # Using the client as a context manager runs the lifespan (DB init).
    with TestClient(api_app) as test_client:
        yield test_client
