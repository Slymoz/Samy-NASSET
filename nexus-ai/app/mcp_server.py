"""MCP server exposing Nexus AI to other AI applications.

Any MCP-compatible client (Claude Desktop, Claude Code, Cursor, ...) can use
these tools to search the indexed documents, read them, add notes, browse
past conversations, or delegate a question to the local LLM.

Transports:
  * streamable-http (default): http://127.0.0.1:8001/mcp
  * stdio: for clients that launch the server themselves
           (``python -m app.mcp_server --transport stdio``)
"""

import argparse

from mcp.server.fastmcp import FastMCP

from app import config, llm, rag, storage

mcp = FastMCP(
    "Nexus",
    instructions=(
        "Nexus is a private, local knowledge base. Use search_documents to "
        "find relevant passages, read_document to read a whole file, and "
        "add_note to save information for later. ask_nexus delegates a "
        "question to the local LLM (slow, small model): prefer "
        "search_documents and reason over the results yourself."
    ),
    host=config.MCP_HOST,
    port=config.MCP_PORT,
)


@mcp.tool()
def list_documents() -> list[dict]:
    """List indexed documents (id, filename and number of passages)."""
    return rag.list_documents()


@mcp.tool()
def search_documents(
    query: str, limit: int = 4, document_id: str | None = None
) -> list[dict]:
    """Semantic search over the indexed documents.

    Returns the closest passages with filename, passage number and distance
    (lower = more relevant). Pass ``document_id`` to search a single document.
    """
    return rag.search_documents(query, limit, document_id)


@mcp.tool()
def read_document(
    document_id: str, start_chunk: int = 0, max_chunks: int = 10
) -> dict:
    """Read a document passage by passage, in order.

    Use ``start_chunk`` and ``max_chunks`` to page through long documents;
    ``total_chunks`` tells you when you have reached the end.
    """
    chunks = rag.get_document_chunks(document_id)
    if not chunks:
        raise ValueError(f"Unknown document: {document_id}")

    start = max(0, start_chunk)
    selected = chunks[start : start + max(1, min(max_chunks, 50))]
    return {
        "document_id": document_id,
        "filename": chunks[0]["filename"],
        "total_chunks": len(chunks),
        "passages": selected,
    }


@mcp.tool()
def add_note(title: str, content: str) -> dict:
    """Save a text note into the knowledge base so it becomes searchable."""
    if not content.strip():
        raise ValueError("The note is empty.")
    return rag.index_text(title, content)


@mcp.tool()
def delete_document(document_id: str) -> dict:
    """Permanently remove a document (or note) from the knowledge base."""
    if not rag.delete_document(document_id):
        raise ValueError(f"Unknown document: {document_id}")
    return {"deleted": document_id}


@mcp.tool()
def list_conversations() -> list[dict]:
    """List past Nexus conversations, most recent first."""
    return storage.list_conversations()


@mcp.tool()
def get_conversation(conversation_id: int, limit: int = 50) -> list[dict]:
    """Return the last ``limit`` messages of a conversation."""
    if not storage.conversation_exists(conversation_id):
        raise ValueError(f"Unknown conversation: {conversation_id}")
    return storage.get_messages(conversation_id)[-max(1, limit):]


@mcp.tool()
def ask_nexus(question: str, use_documents: bool = True) -> dict:
    """Ask the local LLM a question, optionally grounded in the documents.

    Stateless: the exchange is not saved to any conversation.
    """
    sources = rag.search_documents(question) if use_documents else []
    return {
        "answer": llm.generate_answer(question, sources=sources),
        "model": config.CHAT_MODEL,
        "sources": [
            {"filename": s["filename"], "chunk": s["chunk"]} for s in sources
        ],
    }


def main() -> None:
    parser = argparse.ArgumentParser(description="Nexus MCP server")
    parser.add_argument(
        "--transport",
        choices=["streamable-http", "stdio"],
        default="streamable-http",
    )
    args = parser.parse_args()

    # Make sure the schema exists even if the API was never started.
    storage.init_db()
    # With stdio, stdout carries the protocol: never print() in this module.
    mcp.run(transport=args.transport)


if __name__ == "__main__":
    main()
