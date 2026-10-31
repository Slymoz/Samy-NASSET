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


def _with_context(question: str, sources: Iterable[dict]) -> str:
    """Wrap the question with the retrieved passages."""
    context = "\n\n".join(
        f"[Source: {s['filename']}, passage {s['chunk'] + 1}]\n{s['text']}"
        for s in sources
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
    """
    sources = list(sources)
    messages = [{"role": "system", "content": SYSTEM_PROMPT}]
    messages.extend(
        {"role": m["role"], "content": m["content"]}
        for m in history
        if m["role"] in ("user", "assistant")
    )
    messages.append(
        {
            "role": "user",
            "content": _with_context(question, sources) if sources else question,
        }
    )
    return messages


def generate_answer(
    question: str, history: Iterable[dict] = (), sources: Iterable[dict] = ()
) -> str:
    """Blocking, non-streaming completion."""
    response = httpx.post(
        f"{config.OLLAMA_URL}/api/chat",
        json={
            "model": config.CHAT_MODEL,
            "messages": build_messages(question, history, sources),
            "stream": False,
        },
        timeout=180,
    )
    response.raise_for_status()
    return response.json()["message"]["content"]


async def stream_chat(messages: list[dict]) -> AsyncIterator[dict]:
    """Yield Ollama's raw streaming chunks (one JSON object per line)."""
    async with httpx.AsyncClient(timeout=None) as client:
        async with client.stream(
            "POST",
            f"{config.OLLAMA_URL}/api/chat",
            json={"model": config.CHAT_MODEL, "messages": messages, "stream": True},
        ) as response:
            response.raise_for_status()
            async for line in response.aiter_lines():
                if line:
                    yield json.loads(line)
