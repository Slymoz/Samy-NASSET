import json

import httpx
import pytest

from app import __version__, config, llm, main, rag


def _events(response) -> list[dict]:
    return [json.loads(line) for line in response.text.splitlines() if line]


def _upload(client, name: str, content: bytes):
    return client.post("/api/documents", files={"file": (name, content)})


# --------------------------------------------------------------------------- #
# Health and status
# --------------------------------------------------------------------------- #
def test_api_basics(client):
    assert client.get("/").json()["docs"] == "/docs"
    health = client.get("/api/health").json()
    assert health["status"] == "ok"
    assert health["version"] == __version__

    created = client.post("/api/conversations").json()
    assert client.get(f"/api/conversations/{created['id']}/messages").json() == []
    assert client.get("/api/conversations/999999/messages").status_code == 404

    assert client.get("/api/documents").status_code == 200
    bad_upload = _upload(client, "x.exe", b"data")
    assert bad_upload.status_code == 400

    assert client.delete(f"/api/conversations/{created['id']}").status_code == 200
    assert client.delete(f"/api/conversations/{created['id']}").status_code == 404


def test_status_when_ollama_is_down(client, monkeypatch):
    monkeypatch.setattr(config, "OLLAMA_URL", "http://127.0.0.1:9")  # nothing listens
    monkeypatch.setattr(config, "MCP_PORT", 9)
    status = client.get("/api/status").json()
    assert status["api"] is True
    assert status["ollama"] is status["model"] is status["mcp"] is False
    assert status["model_name"] == config.CHAT_MODEL


def test_status_detects_installed_models(client, monkeypatch):
    real = httpx.AsyncClient

    def handler(request):
        return httpx.Response(200, json={"models": [{"name": f"{config.CHAT_MODEL}"}]})

    monkeypatch.setattr(
        main.httpx,
        "AsyncClient",
        lambda **kw: real(transport=httpx.MockTransport(handler), **kw),
    )
    status = client.get("/api/status").json()
    assert status["ollama"] is True
    assert status["model"] is True
    assert status["embedding_model"] is False


def test_probe_host_maps_wildcards_to_loopback():
    assert main._probe_host("0.0.0.0") == "127.0.0.1"
    assert main._probe_host("192.168.1.2") == "192.168.1.2"


# --------------------------------------------------------------------------- #
# Conversations
# --------------------------------------------------------------------------- #
def test_rename_and_export_conversation(client, fake_llm):
    cid = client.post("/api/conversations").json()["id"]
    client.post("/api/chat", json={"message": "Hello?", "conversation_id": cid})

    renamed = client.patch(f"/api/conversations/{cid}", json={"title": "Greetings"})
    assert renamed.status_code == 200
    assert renamed.json()["title"] == "Greetings"
    assert client.patch(f"/api/conversations/{cid}", json={"title": "  "}).status_code == 400
    assert client.patch("/api/conversations/999999", json={"title": "x"}).status_code == 404

    export = client.get(f"/api/conversations/{cid}/export")
    assert export.status_code == 200
    assert export.headers["content-type"].startswith("text/markdown")
    assert "attachment" in export.headers["content-disposition"]
    assert export.text.startswith("# Greetings")
    assert "## You\n\nHello?" in export.text
    assert "## Nexus\n\nHello from Nexus" in export.text
    assert client.get("/api/conversations/999999/export").status_code == 404


def test_markdown_export_lists_sources():
    markdown = main.conversation_to_markdown(
        {"title": "T"},
        [
            {"role": "user", "content": "q"},
            {"role": "assistant", "content": "a", "sources": [{"filename": "f.md", "chunk": 1}]},
        ],
    )
    assert "> Sources: f.md (passage 2)" in markdown


# --------------------------------------------------------------------------- #
# Documents
# --------------------------------------------------------------------------- #
def test_document_lifecycle(client, fake_embeddings):
    uploaded = _upload(client, "guide.md", b"# Guide\nRun ./deploy.sh to deploy.")
    assert uploaded.status_code == 200
    doc = uploaded.json()
    assert doc["filename"] == "guide.md" and doc["duplicate"] is False

    again = _upload(client, "copy.md", b"# Guide\nRun ./deploy.sh to deploy.").json()
    assert again["duplicate"] is True and again["document_id"] == doc["document_id"]

    listed = client.get("/api/documents").json()
    assert [d["document_id"] for d in listed] == [doc["document_id"]]

    read = client.get(f"/api/documents/{doc['document_id']}").json()
    assert read["filename"] == "guide.md"
    assert read["passages"][0]["text"].startswith("# Guide")
    assert client.get("/api/documents/unknown").status_code == 404

    found = client.get("/api/search", params={"q": "how to deploy"}).json()
    assert found[0]["document_id"] == doc["document_id"]
    assert client.get("/api/search", params={"q": ""}).status_code == 422
    assert client.get("/api/search", params={"q": "x", "limit": 99}).status_code == 422

    assert client.delete(f"/api/documents/{doc['document_id']}").status_code == 200
    assert client.delete(f"/api/documents/{doc['document_id']}").status_code == 404
    assert client.get("/api/documents").json() == []


def test_upload_size_limit(client, monkeypatch, fake_embeddings):
    monkeypatch.setattr(config, "MAX_UPLOAD_BYTES", 10)
    response = _upload(client, "big.txt", b"x" * 11)
    assert response.status_code == 400
    assert "too large" in response.json()["detail"]
    assert _upload(client, "ok.txt", b"x" * 10).status_code == 200


def test_upload_reports_ollama_failures(client, monkeypatch):
    def broken(texts):
        raise httpx.ConnectError("connection refused")

    monkeypatch.setattr(rag, "embed_texts", broken)
    response = _upload(client, "note.txt", b"hello")
    assert response.status_code == 502
    assert config.EMBED_MODEL in response.json()["detail"]
    assert list(config.UPLOAD_DIR.iterdir()) == []  # nothing half-indexed is kept


# --------------------------------------------------------------------------- #
# Chat
# --------------------------------------------------------------------------- #
def test_chat_creates_conversation_and_persists_sources(client, fake_llm, fake_embeddings):
    _upload(client, "deploy.md", b"Use the deploy script to deploy the app.")

    response = client.post("/api/chat", json={"message": "  How to deploy?  "})
    assert response.status_code == 200
    body = response.json()
    assert body["answer"] == "Hello from Nexus"
    assert body["sources"][0]["filename"] == "deploy.md"

    messages = client.get(f"/api/conversations/{body['conversation_id']}/messages").json()
    assert [m["content"] for m in messages] == ["How to deploy?", "Hello from Nexus"]
    assert messages[1]["sources"][0]["filename"] == "deploy.md"
    assert "deploy script" in fake_llm.calls[0][-1]["content"]  # context was injected


def test_chat_without_documents_skips_retrieval(client, fake_llm, fake_embeddings):
    _upload(client, "deploy.md", b"Deploy notes.")
    fake_embeddings.clear()

    body = client.post("/api/chat", json={"message": "hi", "use_documents": False}).json()
    assert body["sources"] == []
    assert fake_embeddings == []  # no embedding call at all
    assert fake_llm.calls[0][-1]["content"] == "hi"


def test_chat_sends_history_to_the_model(client, fake_llm, monkeypatch):
    monkeypatch.setattr(config, "HISTORY_MESSAGES", 2)
    cid = client.post("/api/conversations").json()["id"]
    for text in ("one", "two", "three"):
        client.post("/api/chat", json={"message": text, "conversation_id": cid})

    roles_contents = [(m["role"], m["content"]) for m in fake_llm.calls[-1][1:]]
    assert roles_contents == [("user", "two"), ("assistant", "Hello from Nexus"), ("user", "three")]


@pytest.mark.parametrize(
    "payload, code",
    [
        ({"message": "   "}, 400),
        ({"message": ""}, 422),
        ({"message": "x" * 20001}, 422),
        ({"message": "hi", "conversation_id": 999999}, 404),
    ],
)
def test_chat_rejects_invalid_requests(client, fake_llm, payload, code):
    assert client.post("/api/chat", json=payload).status_code == code
    assert client.post("/api/chat/stream", json=payload).status_code == code


def test_chat_reports_ollama_errors(client, fake_llm):
    fake_llm.error = llm.OllamaError("Ollama error 404: model not found")
    response = client.post("/api/chat", json={"message": "hi"})
    assert response.status_code == 502
    assert "model not found" in response.json()["detail"]
    # The question is kept even though the answer failed.
    cid = client.get("/api/conversations").json()[0]["id"]
    assert [m["role"] for m in client.get(f"/api/conversations/{cid}/messages").json()] == ["user"]


def test_chat_stream_events_and_persistence(client, fake_llm, fake_embeddings):
    _upload(client, "a.md", b"Nexus streams tokens.")
    response = client.post("/api/chat/stream", json={"message": "Does Nexus stream?"})
    assert response.status_code == 200
    assert response.headers["content-type"].startswith("application/x-ndjson")

    events = _events(response)
    kinds = [e["type"] for e in events]
    assert kinds == ["meta", "token", "token", "token", "done"]
    assert events[0]["sources"][0]["filename"] == "a.md"
    assert "".join(e["text"] for e in events if e["type"] == "token") == "Hello from Nexus"
    done = events[-1]
    assert done["generated_tokens"] == 3
    assert done["tokens_per_second"] == 2.0

    cid = events[0]["conversation_id"]
    messages = client.get(f"/api/conversations/{cid}/messages").json()
    assert [m["content"] for m in messages] == ["Does Nexus stream?", "Hello from Nexus"]
    assert messages[1]["sources"][0]["filename"] == "a.md"


def test_chat_stream_reports_errors_as_events(client, fake_llm):
    fake_llm.error = RuntimeError("model crashed")
    events = _events(client.post("/api/chat/stream", json={"message": "hi"}))

    assert events[0]["type"] == "meta"
    assert events[-1]["type"] == "error"
    assert "model crashed" in events[-1]["message"]
    messages = client.get(f"/api/conversations/{events[0]['conversation_id']}/messages").json()
    assert [m["role"] for m in messages] == ["user"]  # no half answer saved


def test_chat_stream_reports_ollama_error_chunks(client, monkeypatch):
    async def failing_stream(messages):
        yield {"error": "model 'nope' not found"}

    monkeypatch.setattr(llm, "stream_chat", failing_stream)
    events = _events(
        client.post("/api/chat/stream", json={"message": "hi", "use_documents": False})
    )
    assert events[-1] == {"type": "error", "message": "RuntimeError: model 'nope' not found"}


def test_chat_returns_502_when_embeddings_fail(client, fake_llm, fake_embeddings, monkeypatch):
    _upload(client, "a.md", b"content")

    def broken(texts):
        raise httpx.ConnectError("refused")

    monkeypatch.setattr(rag, "embed_texts", broken)
    assert client.post("/api/chat", json={"message": "hi"}).status_code == 502
    assert client.post("/api/chat/stream", json={"message": "hi"}).status_code == 502
