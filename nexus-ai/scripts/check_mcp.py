"""Smoke test for the MCP server: connect over HTTP, list and call tools.

With ``--with-ollama`` it also adds a note through MCP and checks that the
API (another process) finds it, then deletes it. This needs Ollama and the
embedding model.
"""

import asyncio
import json
import os
import sys

import httpx
from mcp import ClientSession
from mcp.client.streamable_http import streamable_http_client

URL = os.getenv("NEXUS_MCP_URL", "http://127.0.0.1:8001/mcp")
API_URL = os.getenv("NEXUS_API_URL", "http://127.0.0.1:8000")
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


async def check() -> None:
    async with (
        streamable_http_client(URL) as (read, write, _),
        ClientSession(read, write) as session,
    ):
        await session.initialize()

        tools = {tool.name for tool in (await session.list_tools()).tools}
        missing = EXPECTED_TOOLS - tools
        if missing:
            raise SystemExit(f"Missing MCP tools: {sorted(missing)}")

        # A real call that exercises ChromaDB without needing Ollama.
        result = await session.call_tool("list_documents", {})
        if result.isError:
            raise SystemExit(f"list_documents failed: {result.content}")

    print(f"OK  MCP server ({len(tools)} tools)")


async def check_shared_index() -> None:
    """A note added through MCP must be searchable through the API."""
    async with (
        streamable_http_client(URL) as (read, write, _),
        ClientSession(read, write) as session,
    ):
        await session.initialize()
        result = await session.call_tool(
            "add_note", {"title": "mcp-smoke", "content": "The MCP smoke note mentions zeppelins."}
        )
        if result.isError:
            raise SystemExit(f"add_note failed: {result.content}")
        note = json.loads(result.content[0].text)

        try:
            found = httpx.get(f"{API_URL}/api/search", params={"q": "zeppelins"}, timeout=60)
            found.raise_for_status()
            if note["document_id"] not in {r["document_id"] for r in found.json()}:
                raise SystemExit("A note added through MCP is not visible from the API.")
        finally:
            await session.call_tool("delete_document", {"document_id": note["document_id"]})

    print("OK  note added through MCP is found by the API (shared index)")


async def main() -> None:
    # The server may still be starting: retry for up to 30 seconds.
    for attempt in range(30):
        try:
            await check()
            if "--with-ollama" in sys.argv:
                await check_shared_index()
            return
        except SystemExit:
            raise
        except Exception as exc:
            if attempt == 29:
                raise
            print(f"MCP not ready yet ({type(exc).__name__}), retrying...")
            await asyncio.sleep(1)


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except Exception as exc:
        print(f"FAIL MCP server: {exc!r}")
        sys.exit(1)
