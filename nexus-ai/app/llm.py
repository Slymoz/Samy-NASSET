"""Prompt construction and Ollama chat client."""

import json
from collections.abc import AsyncIterator, Iterable

import httpx

from app import config

SYSTEM_PROMPT = (
    "You are Nexus, a helpful personal assistant. "
    "Answer clearly and reply in the language of the user's question. "
    "If document excerpts are provided, use them carefully. "
    "Never invent quotes or facts supposedly taken from the sources. "
    "If the documents do not contain the answer, say so."
)


class OllamaError(httpx.HTTPError):
    """Ollama answered with an error (unknown model, out of memory, ...).

    Subclasses ``httpx.HTTPError`` so callers can handle every Ollama failure,
    network or application level, with a single ``except``.
    """


def _with_context(question: str, sources: Iterable[dict]) -> str:
    """Wrap the question with the retrieved passages."""
    context = "\n\n".join(
        f"[Source: {s['filename']}, passage {s['chunk'] + 1}]\n{s['text']}" for s in sources
    )
    return (
        "Here are excerpts retrieved from the user's documents. "
        "They may help, but are not necessarily sufficient.\n\n"
        f"{context}\n\n"
        f"Current question:\n{question}\n\n"
        "Take these excerpts into account and name the file when you use one. "
        "Do not claim a source says something it does not."
    )


def build_messages(
    question: str, history: Iterable[dict] = (), sources: Iterable[dict] = ()
) -> list[dict]:
    """Assemble the message list sent to the model.

    Only the plain question is stored in the history; the retrieved context
    is injected for the current turn only, so it never bloats later prompts.
    The history is trimmed to the last ``NEXUS_HISTORY_MESSAGES`` messages.
    """
    sources = list(sources)
    turns = [m for m in history if m["role"] in ("user", "assistant")]
    keep = config.HISTORY_MESSAGES
    turns = turns[-keep:] if keep else []

    messages = [{"role": "system", "content": SYSTEM_PROMPT}]
    messages.extend({"role": m["role"], "content": m["content"]} for m in turns)
    messages.append(
        {
            "role": "user",
            "content": _with_context(question, sources) if sources else question,
        }
    )
    return messages


def _payload(messages: list[dict], stream: bool) -> dict:
    payload = {"model": config.CHAT_MODEL, "messages": messages, "stream": stream}
    if config.TEMPERATURE is not None:
        payload["options"] = {"temperature": config.TEMPERATURE}
    return payload


def _error_detail(response: httpx.Response) -> str:
    """Extract Ollama's error message (it answers ``{"error": "..."}``)."""
    try:
        return response.json().get("error") or response.text
    except ValueError:
        return response.text


def _raise_for_ollama(response: httpx.Response) -> None:
    if response.is_error:
        raise OllamaError(f"Ollama error {response.status_code}: {_error_detail(response)}")


def generate_answer(
    question: str, history: Iterable[dict] = (), sources: Iterable[dict] = ()
) -> str:
    """Blocking, non-streaming completion."""
    response = httpx.post(
        f"{config.OLLAMA_URL}/api/chat",
        json=_payload(build_messages(question, history, sources), stream=False),
        timeout=config.LLM_TIMEOUT,
    )
    _raise_for_ollama(response)
    return response.json()["message"]["content"]


async def stream_chat(messages: list[dict]) -> AsyncIterator[dict]:
    """Yield Ollama's raw streaming chunks (one JSON object per line).

    The read timeout applies between two chunks, so a slow but progressing
    generation is never cut, while a stalled Ollama does not hang forever.
    """
    timeout = httpx.Timeout(config.LLM_TIMEOUT, connect=10.0)
    async with (
        httpx.AsyncClient(timeout=timeout) as client,
        client.stream(
            "POST",
            f"{config.OLLAMA_URL}/api/chat",
            json=_payload(messages, stream=True),
        ) as response,
    ):
        if response.is_error:
            await response.aread()
            _raise_for_ollama(response)
        async for line in response.aiter_lines():
            if line:
                yield json.loads(line)
