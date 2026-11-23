import asyncio
import functools
import json

import httpx
import pytest

from app import config, llm

SOURCE = {"filename": "guide.md", "chunk": 0, "text": "Deploy with ./deploy.sh"}


def test_build_messages_without_sources():
    messages = llm.build_messages("Hi?")
    assert messages[0] == {"role": "system", "content": llm.SYSTEM_PROMPT}
    assert messages[-1] == {"role": "user", "content": "Hi?"}


def test_build_messages_injects_sources_for_the_current_turn_only():
    history = [
        {"role": "user", "content": "q1", "id": 1},
        {"role": "assistant", "content": "a1", "sources": []},
        {"role": "system", "content": "ignored"},
    ]
    messages = llm.build_messages("How to deploy?", history, [SOURCE])

    assert [m["role"] for m in messages] == ["system", "user", "assistant", "user"]
    assert messages[1] == {"role": "user", "content": "q1"}  # extra keys dropped
    assert "[Source: guide.md, passage 1]" in messages[-1]["content"]
    assert "How to deploy?" in messages[-1]["content"]


def test_build_messages_trims_history(monkeypatch):
    history = [{"role": "user", "content": f"m{i}"} for i in range(10)]

    monkeypatch.setattr(config, "HISTORY_MESSAGES", 3)
    contents = [m["content"] for m in llm.build_messages("now", history)]
    assert contents[1:] == ["m7", "m8", "m9", "now"]

    monkeypatch.setattr(config, "HISTORY_MESSAGES", 0)
    assert len(llm.build_messages("now", history)) == 2


def _mock_post(handler):
    def post(url, json, timeout):
        request = httpx.Request("POST", url, json=json)
        return handler(request, json)

    return post


def test_generate_answer_sends_model_and_options(monkeypatch):
    sent = {}

    def handler(request, payload):
        sent.update(payload)
        return httpx.Response(200, json={"message": {"content": "42"}}, request=request)

    monkeypatch.setattr(llm.httpx, "post", _mock_post(handler))
    monkeypatch.setattr(config, "TEMPERATURE", 0.2)

    assert llm.generate_answer("question") == "42"
    assert sent["model"] == config.CHAT_MODEL
    assert sent["stream"] is False
    assert sent["options"] == {"temperature": 0.2}


def test_generate_answer_reports_ollama_errors(monkeypatch):
    def handler(request, payload):
        return httpx.Response(404, json={"error": "model 'x' not found"}, request=request)

    monkeypatch.setattr(llm.httpx, "post", _mock_post(handler))
    with pytest.raises(llm.OllamaError, match="model 'x' not found"):
        llm.generate_answer("question")
    assert issubclass(llm.OllamaError, httpx.HTTPError)


def _patch_async_client(monkeypatch, handler):
    real = httpx.AsyncClient
    transport = httpx.MockTransport(handler)
    monkeypatch.setattr(llm.httpx, "AsyncClient", functools.partial(real, transport=transport))


async def _collect(messages):
    return [chunk async for chunk in llm.stream_chat(messages)]


def test_stream_chat_yields_ollama_chunks(monkeypatch):
    lines = [{"message": {"content": "Hel"}}, {"message": {"content": "lo"}, "done": True}]

    def handler(request):
        assert json.loads(request.content)["stream"] is True
        body = "\n".join(json.dumps(line) for line in lines) + "\n\n"
        return httpx.Response(200, text=body)

    _patch_async_client(monkeypatch, handler)
    assert asyncio.run(_collect([{"role": "user", "content": "hi"}])) == lines


def test_stream_chat_reports_ollama_errors(monkeypatch):
    def handler(request):
        return httpx.Response(500, json={"error": "out of memory"})

    _patch_async_client(monkeypatch, handler)
    with pytest.raises(llm.OllamaError, match="out of memory"):
        asyncio.run(_collect([]))
