import asyncio
import json
import os
import time

import streamlit as st
from mcp import ClientSession
from mcp.client.streamable_http import streamablehttp_client

DEFAULT_URL = os.getenv("NEXUS_MCP_URL", "http://127.0.0.1:8001/mcp")

st.set_page_config(page_title="Nexus MCP", page_icon="🔌", layout="wide")


# --------------------------------------------------------------------------- #
# MCP client helpers
# --------------------------------------------------------------------------- #
async def _run(url, action):
    """Open a session, run one action on it, close the session."""
    async with streamablehttp_client(url) as (read, write, _):
        async with ClientSession(read, write) as session:
            await session.initialize()
            return await action(session)


def run(url, action):
    # Streamlit scripts have no running event loop, so asyncio.run is safe.
    return asyncio.run(_run(url, action))


def load_tools(url):
    try:
        st.session_state.mcp_tools = run(url, lambda s: s.list_tools()).tools
    except Exception as exc:  # connection errors surface as exception groups
        st.session_state.mcp_tools = []
        st.error(f"Cannot reach the MCP server at {url}. Is ./start.sh running?")
        st.caption(f"{type(exc).__name__}")


# --------------------------------------------------------------------------- #
# Form generation from a tool's JSON schema
# --------------------------------------------------------------------------- #
def schema_type(prop):
    """Return the JSON type of a property, ignoring 'null' in optional types."""
    if "type" in prop:
        return prop["type"]
    for option in prop.get("anyOf", []):
        if option.get("type") != "null":
            return option["type"]
    return "string"


def render_field(tool_name, name, prop, required):
    label = f"{name} *" if required else name
    key = f"{tool_name}_{name}"
    kind = schema_type(prop)

    if kind == "boolean":
        return st.checkbox(label, value=prop.get("default", False), key=key)
    if kind == "integer":
        return st.number_input(label, value=int(prop.get("default", 1)), step=1, key=key)
    return st.text_area(label, height=68, key=key)


def render_result(result, elapsed):
    if result.isError:
        st.error("The tool returned an error.")
    else:
        st.success(f"Done in {elapsed:.2f} s")

    structured = getattr(result, "structuredContent", None)
    if structured is not None:
        st.json(structured.get("result", structured))
        return

    for block in result.content:
        text = getattr(block, "text", None)
        if text is None:
            continue
        try:
            st.json(json.loads(text))
        except ValueError:
            st.write(text)


# --------------------------------------------------------------------------- #
# Page
# --------------------------------------------------------------------------- #
st.title("🔌 Nexus MCP")
st.caption("Try the tools that external AI apps (Claude, Cursor...) can use.")

url = st.text_input("MCP server URL", DEFAULT_URL)

if st.button("Connect / refresh tools") or "mcp_tools" not in st.session_state:
    load_tools(url)

tools = st.session_state.get("mcp_tools", [])
if not tools:
    st.stop()

st.success(f"Connected: {len(tools)} tools available.")

tool = st.selectbox("Tool", tools, format_func=lambda t: t.name)
st.caption((tool.description or "").strip().split("\n\n")[0])

schema = tool.inputSchema or {}
properties = schema.get("properties", {})
required = set(schema.get("required", []))

with st.form(f"form_{tool.name}"):
    if not properties:
        st.caption("This tool takes no parameters.")
    values = {
        name: render_field(tool.name, name, prop, name in required)
        for name, prop in properties.items()
    }
    submitted = st.form_submit_button("Run tool")

if submitted:
    arguments, missing = {}, []
    for name, value in values.items():
        if value == "":
            if name in required:
                missing.append(name)
            continue  # optional and empty: do not send it
        arguments[name] = value

    if missing:
        st.warning(f"Required: {', '.join(missing)}")
    else:
        with st.expander("Request sent", expanded=False):
            st.json({"tool": tool.name, "arguments": arguments})
        try:
            start = time.perf_counter()
            with st.spinner("Calling the tool..."):
                result = run(url, lambda s: s.call_tool(tool.name, arguments))
            render_result(result, time.perf_counter() - start)
        except Exception as exc:
            st.error(f"Call failed: {type(exc).__name__}: {exc}")
