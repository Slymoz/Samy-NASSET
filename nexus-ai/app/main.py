"""FastAPI backend: chat (with streaming), conversations and documents."""

import asyncio
import json
import time
from contextlib import asynccontextmanager

import httpx
from fastapi import FastAPI, File, HTTPException, UploadFile
from pydantic import BaseModel, Field
from starlette.concurrency import run_in_threadpool
from starlette.responses import StreamingResponse

from app import config, llm, rag, storage


@asynccontextmanager
async def lifespan(_: FastAPI):
    storage.init_db()
    yield


app = FastAPI(title="Nexus AI", lifespan=lifespan)


class ChatRequest(BaseModel):
    message: str = Field(min_length=1, max_length=20000)
    conversation_id: int | None = None


# Plain `def` endpoints below run in FastAPI's thread pool, so blocking calls
# (SQLite, ChromaDB, synchronous httpx) never freeze the event loop.


# --------------------------------------------------------------------------- #
# Health and status
# --------------------------------------------------------------------------- #
@app.get("/")
def root():
    return {"service": "Nexus AI", "docs": "/docs", "health": "/api/health"}


@app.get("/api/health")
def health():
    """Liveness probe: answers as soon as the API is up (no Ollama needed)."""
    return {"status": "ok", "service": "FastAPI", "model": config.CHAT_MODEL}


@app.get("/api/status")
async def status():
    """Report which dependent services are reachable."""
    result = {
        "api": True,
        "ollama": False,
        "model": False,
        "embedding_model": False,
        "mcp": False,
        "model_name": config.CHAT_MODEL,
    }

    # Is Ollama running, and are the required models pulled?
    try:
        async with httpx.AsyncClient(timeout=3.0) as client:
            response = await client.get(f"{config.OLLAMA_URL}/api/tags")
            response.raise_for_status()
            names = [m.get("name", "") for m in response.json().get("models", [])]

        def installed(model: str) -> bool:
            return any(n == model or n.startswith(model + ":") for n in names)

        result["ollama"] = True
        result["model"] = installed(config.CHAT_MODEL)
        result["embedding_model"] = installed(config.EMBED_MODEL)
    except httpx.HTTPError:
        pass

    # Is the MCP server accepting TCP connections?
    try:
        _, writer = await asyncio.wait_for(
            asyncio.open_connection(config.MCP_HOST, config.MCP_PORT),
            timeout=1.0,
        )
        result["mcp"] = True
        writer.close()
        await writer.wait_closed()
    except (OSError, asyncio.TimeoutError):
        pass

    return result


# --------------------------------------------------------------------------- #
# Conversations
# --------------------------------------------------------------------------- #
@app.get("/api/conversations")
def list_conversations():
    return storage.list_conversations()


@app.post("/api/conversations")
def create_conversation():
    return {"id": storage.create_conversation(), "title": storage.DEFAULT_TITLE}


@app.delete("/api/conversations/{conversation_id}")
def delete_conversation(conversation_id: int):
    if not storage.delete_conversation(conversation_id):
        raise HTTPException(404, "Conversation not found.")
    return {"deleted": conversation_id}


@app.get("/api/conversations/{conversation_id}/messages")
def conversation_messages(conversation_id: int):
    if not storage.conversation_exists(conversation_id):
        raise HTTPException(404, "Conversation not found.")
    return storage.get_messages(conversation_id)


# --------------------------------------------------------------------------- #
# Documents (RAG)
# --------------------------------------------------------------------------- #
@app.get("/api/documents")
def list_documents():
    try:
        return rag.list_documents()
    except Exception as exc:
        raise HTTPException(500, f"Cannot read the document index: {exc}") from exc


@app.post("/api/documents")
async def upload_document(file: UploadFile = File(...)):
    try:
        content = await file.read()
        if len(content) > config.MAX_UPLOAD_BYTES:
            raise ValueError(
                f"File too large (limit: {config.MAX_UPLOAD_BYTES // 2**20} MB)."
            )
        return await run_in_threadpool(
            rag.index_file, file.filename or "document.txt", content
        )
    except ValueError as exc:
        raise HTTPException(400, str(exc)) from exc
    except httpx.HTTPError as exc:
        raise HTTPException(
            502,
            "Could not compute embeddings through Ollama. Make sure Ollama is "
            f"running and '{config.EMBED_MODEL}' is installed.",
        ) from exc
    except Exception as exc:
        raise HTTPException(500, f"Indexing failed: {exc}") from exc
    finally:
        await file.close()


@app.delete("/api/documents/{document_id}")
def delete_document(document_id: str):
    if not rag.delete_document(document_id):
        raise HTTPException(404, "Document not found.")
    return {"deleted": document_id}


# --------------------------------------------------------------------------- #
# Chat
# --------------------------------------------------------------------------- #
def _resolve_conversation(conversation_id: int | None) -> int:
    """Return an existing conversation id, creating one if none was given."""
    if conversation_id is None:
        return storage.create_conversation()
    if not storage.conversation_exists(conversation_id):
        raise HTTPException(404, "Conversation not found.")
    return conversation_id


def _search_or_raise(message: str) -> list[dict]:
    """Retrieve relevant passages, translating failures into HTTP errors."""
    try:
        return rag.search_documents(message)
    except httpx.HTTPError as exc:
        raise HTTPException(
            502,
            "Could not compute embeddings. Check Ollama and "
            f"'{config.EMBED_MODEL}'.",
        ) from exc
    except Exception as exc:
        raise HTTPException(500, f"Document search failed: {exc}") from exc


@app.post("/api/chat")
def chat(request: ChatRequest):
    """Non-streaming chat. Handy for scripts; the UI uses /api/chat/stream."""
    message = request.message.strip()
    if not message:
        raise HTTPException(400, "Empty message.")

    conversation_id = _resolve_conversation(request.conversation_id)
    history = storage.get_messages(conversation_id)
    sources = _search_or_raise(message)

    # Save the question first so it is kept even if generation fails.
    storage.add_message(conversation_id, "user", message)

    try:
        answer = llm.generate_answer(message, history, sources)
    except httpx.HTTPError as exc:
        raise HTTPException(502, f"Ollama communication error: {exc}") from exc

    storage.add_message(conversation_id, "assistant", answer)
    return {
        "answer": answer,
        "conversation_id": conversation_id,
        "sources": sources,
    }


def _event(payload: dict) -> str:
    """Serialize one NDJSON event (one JSON object per line)."""
    return json.dumps(payload, ensure_ascii=False) + "\n"


@app.post("/api/chat/stream")
async def chat_stream(request: ChatRequest):
    """Streaming chat as NDJSON events: meta, token*, then done or error."""
    message = request.message.strip()
    if not message:
        raise HTTPException(400, "Empty message.")

    conversation_id = await run_in_threadpool(
        _resolve_conversation, request.conversation_id
    )
    history = await run_in_threadpool(storage.get_messages, conversation_id)
    sources = await run_in_threadpool(_search_or_raise, message)
    messages = llm.build_messages(message, history, sources)
    await run_in_threadpool(storage.add_message, conversation_id, "user", message)

    async def generate():
        start = time.perf_counter()
        answer_parts: list[str] = []
        final_chunk: dict = {}

        yield _event(
            {
                "type": "meta",
                "conversation_id": conversation_id,
                "model": config.CHAT_MODEL,
                "sources": sources,
            }
        )

        try:
            async for chunk in llm.stream_chat(messages):
                if chunk.get("error"):
                    raise RuntimeError(chunk["error"])

                token = chunk.get("message", {}).get("content", "")
                if token:
                    answer_parts.append(token)
                    yield _event({"type": "token", "text": token})

                if chunk.get("done"):
                    final_chunk = chunk
                    break
        except Exception as exc:
            yield _event({"type": "error", "message": f"{type(exc).__name__}: {exc}"})
            return

        await run_in_threadpool(
            storage.add_message,
            conversation_id,
            "assistant",
            "".join(answer_parts),
        )

        # Ollama reports generation time in nanoseconds.
        tokens = final_chunk.get("eval_count")
        duration_ns = final_chunk.get("eval_duration")
        speed = tokens / (duration_ns / 1e9) if tokens and duration_ns else None

        yield _event(
            {
                "type": "done",
                "conversation_id": conversation_id,
                "elapsed_seconds": round(time.perf_counter() - start, 2),
                "generated_tokens": tokens,
                "tokens_per_second": round(speed, 2) if speed else None,
            }
        )

    return StreamingResponse(
        generate(),
        media_type="application/x-ndjson",
        headers={"Cache-Control": "no-cache", "X-Accel-Buffering": "no"},
    )
