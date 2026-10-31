"""Smoke test for the MCP server: connect over HTTP, list and call tools."""

import asyncio
import sys

from mcp import ClientSession
from mcp.client.streamable_http import streamablehttp_client

URL = "http://127.0.0.1:8001/mcp"
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
    async with streamablehttp_client(URL) as (read, write, _):
        async with ClientSession(read, write) as session:
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


async def main() -> None:
    # The server may still be starting: retry for up to 30 seconds.
    for attempt in range(30):
        try:
            await check()
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
