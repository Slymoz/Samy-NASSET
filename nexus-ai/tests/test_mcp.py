"""MCP server tests through a real MCP client session (in memory)."""

import asyncio
import json

from mcp.shared.memory import create_connected_server_and_client_session

from app import mcp_server, storage

EXPECTED_TOOLS = {
    "list_documents",
    "search_documents",
    "read_document",
    "add_note",
    "delete_document",
    "list_conversations",
    "get_conversation",
    "ask_nexus",
}


def _run(action):
    async def go():
        async with create_connected_server_and_client_session(mcp_server.mcp) as session:
            return await action(session)

    return asyncio.run(go())


def _call(name: str, arguments: dict | None = None):
    """Call a tool and return (is_error, decoded result)."""
    result = _run(lambda s: s.call_tool(name, arguments or {}))
    if result.isError:
        return True, result.content[0].text
    structured = result.structuredContent
    if structured is not None:
        return False, structured.get("result", structured)
    return False, json.loads(result.content[0].text)


def test_all_tools_are_exposed_with_annotations():
    tools = {tool.name: tool for tool in _run(lambda s: s.list_tools()).tools}
    assert set(tools) == EXPECTED_TOOLS

    assert tools["search_documents"].annotations.readOnlyHint is True
    assert tools["delete_document"].annotations.destructiveHint is True
    assert tools["add_note"].annotations.readOnlyHint is False
    assert "query" in tools["search_documents"].inputSchema["required"]


def test_note_lifecycle(fake_embeddings):
    error, note = _call("add_note", {"title": "Checklist", "content": "Rollback with ./undo.sh"})
    assert not error
    assert note["filename"] == "Checklist.md"
    doc_id = note["document_id"]

    _, again = _call("add_note", {"title": "Other", "content": "Rollback with ./undo.sh"})
    assert again["duplicate"] is True

    _, documents = _call("list_documents")
    assert [d["document_id"] for d in documents] == [doc_id]

    _, results = _call("search_documents", {"query": "rollback", "limit": 2})
    assert results[0]["document_id"] == doc_id

    _, page = _call("read_document", {"document_id": doc_id, "max_chunks": 1})
    assert page["total_chunks"] == 1
    assert page["passages"][0]["text"] == "Rollback with ./undo.sh"

    _, deleted = _call("delete_document", {"document_id": doc_id})
    assert deleted == {"deleted": doc_id}
    error, message = _call("delete_document", {"document_id": doc_id})
    assert error and "Unknown document" in message


def test_invalid_inputs_return_tool_errors(fake_embeddings):
    assert _call("add_note", {"title": "t", "content": "   "})[0] is True
    assert _call("read_document", {"document_id": "missing"})[0] is True
    assert _call("get_conversation", {"conversation_id": 123456})[0] is True


def test_conversation_tools():
    cid = storage.create_conversation()
    for i in range(4):
        storage.add_message(cid, "user" if i % 2 == 0 else "assistant", f"m{i}")

    _, conversations = _call("list_conversations")
    assert conversations[0]["id"] == cid

    _, messages = _call("get_conversation", {"conversation_id": cid, "limit": 2})
    assert [m["content"] for m in messages] == ["m2", "m3"]


def test_ask_nexus_uses_documents_only_when_asked(fake_embeddings, fake_llm):
    _call("add_note", {"title": "Wifi", "content": "The wifi password is hunter2"})

    _, grounded = _call("ask_nexus", {"question": "wifi password?"})
    assert grounded["answer"] == "Hello from Nexus"
    assert grounded["sources"] == [{"filename": "Wifi.md", "chunk": 0}]
    assert "hunter2" in fake_llm.calls[-1][-1]["content"]

    _, plain = _call("ask_nexus", {"question": "wifi password?", "use_documents": False})
    assert plain["sources"] == []
    assert fake_llm.calls[-1][-1]["content"] == "wifi password?"
