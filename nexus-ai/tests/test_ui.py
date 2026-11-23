"""Streamlit UI tests: the real pages, driven headlessly with AppTest.

The API runs for real (uvicorn in a background thread); only Ollama is faked.
"""

import socket
import threading
import time
from pathlib import Path

import pytest
import uvicorn
from streamlit.testing.v1 import AppTest

from app import mcp_server, rag, storage
from app.main import app as api_app

FRONTEND = Path(__file__).resolve().parent.parent / "frontend"
TIMEOUT = 30


def _free_port() -> int:
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


@pytest.fixture
def live_api(fake_llm, fake_embeddings, monkeypatch):
    port = _free_port()
    server = uvicorn.Server(
        uvicorn.Config(api_app, host="127.0.0.1", port=port, log_level="warning")
    )
    thread = threading.Thread(target=server.run, daemon=True)
    thread.start()
    deadline = time.monotonic() + 15
    while not server.started:
        if time.monotonic() > deadline:
            raise RuntimeError("API did not start")
        time.sleep(0.05)

    monkeypatch.setenv("NEXUS_API_URL", f"http://127.0.0.1:{port}")
    yield fake_llm
    server.should_exit = True
    thread.join(timeout=10)


def _button(at: AppTest, label: str):
    return next(b for b in at.button if b.label == label)


def _texts(elements) -> str:
    return "\n".join(str(e.value) for e in elements)


def test_chat_page_without_api(monkeypatch):
    monkeypatch.setenv("NEXUS_API_URL", "http://127.0.0.1:9")
    at = AppTest.from_file(str(FRONTEND / "streamlit_app.py"), default_timeout=TIMEOUT).run()
    assert not at.exception
    assert "FastAPI is unreachable" in _texts(at.error)


def test_chat_flow_end_to_end(live_api):
    at = AppTest.from_file(str(FRONTEND / "streamlit_app.py"), default_timeout=TIMEOUT).run()
    assert not at.exception
    assert "Create a conversation" in _texts(at.info)

    _button(at, "＋ New conversation").click().run()
    assert not at.exception
    assert at.sidebar.selectbox[0].value is not None

    at.chat_input[0].set_value("Hello Nexus").run()
    assert not at.exception
    rendered = _texts(at.markdown)
    assert "Hello Nexus" in rendered
    assert "Hello from Nexus" in rendered
    assert any("tokens/s" in str(c.value) for c in at.caption)

    at.run()  # next run shows the saved history, with the generated title
    assert "Hello from Nexus" in _texts(at.markdown)
    options = at.sidebar.selectbox[0].options
    assert any(option.startswith("Hello Nexus") for option in options)


def test_rag_toggle_is_sent_to_the_api(live_api):
    at = AppTest.from_file(str(FRONTEND / "streamlit_app.py"), default_timeout=TIMEOUT).run()
    _button(at, "＋ New conversation").click().run()
    at.sidebar.toggle[0].set_value(False).run()
    at.chat_input[0].set_value("plain question").run()

    assert not at.exception
    assert live_api.calls[-1][-1]["content"] == "plain question"  # no injected context


def test_mcp_page_without_server(monkeypatch):
    monkeypatch.setenv("NEXUS_MCP_URL", "http://127.0.0.1:9/mcp")
    at = AppTest.from_file(str(FRONTEND / "pages" / "1_MCP.py"), default_timeout=TIMEOUT).run()
    assert not at.exception
    assert "Cannot reach the MCP server" in _texts(at.error)


def test_documents_sources_and_conversation_management(live_api):
    doc = rag.index_text("deploy guide", "Run the deploy script to ship the app.")
    cid = storage.create_conversation()
    storage.add_message(cid, "user", "How to ship?")
    source = rag.search_documents("deploy script")[0]
    storage.add_message(cid, "assistant", "Use the deploy script.", [source])

    at = AppTest.from_file(str(FRONTEND / "streamlit_app.py"), default_timeout=TIMEOUT).run()
    assert not at.exception
    sidebar_captions = _texts(at.sidebar.caption)
    assert "deploy guide.md" in sidebar_captions
    assert "1 document(s)" in sidebar_captions
    # Sources saved with the answer are shown again in the history.
    assert any("Retrieved sources (1)" in e.label for e in at.expander)

    at.sidebar.text_input[0].set_value("Shipping notes").run()
    _button(at, "Rename").click().run()
    assert storage.get_conversation(cid)["title"] == "Shipping notes"

    next(b for b in at.button if b.key == f"del_{doc['document_id']}").click().run()
    assert rag.list_documents() == []

    _button(at, "Delete this conversation").click().run()
    assert not at.exception
    assert storage.list_conversations() == []
    assert "No conversation yet." in _texts(at.sidebar.caption)


@pytest.fixture
def live_mcp(fake_embeddings, monkeypatch):
    port = _free_port()
    server = uvicorn.Server(
        uvicorn.Config(
            mcp_server.mcp.streamable_http_app(),
            host="127.0.0.1",
            port=port,
            log_level="warning",
        )
    )
    thread = threading.Thread(target=server.run, daemon=True)
    thread.start()
    deadline = time.monotonic() + 15
    while not server.started:
        if time.monotonic() > deadline:
            raise RuntimeError("MCP server did not start")
        time.sleep(0.05)

    monkeypatch.setenv("NEXUS_MCP_URL", f"http://127.0.0.1:{port}/mcp")
    yield
    server.should_exit = True
    thread.join(timeout=10)


def test_mcp_page_calls_a_tool(live_mcp):
    at = AppTest.from_file(str(FRONTEND / "pages" / "1_MCP.py"), default_timeout=TIMEOUT).run()
    assert not at.exception
    assert "8 tools available" in _texts(at.success)

    tool_picker = at.selectbox[0]
    tool_picker.select_index(tool_picker.options.index("add_note")).run()
    at.text_area(key="add_note_title").set_value("From the UI")
    at.text_area(key="add_note_content").set_value("Created through the MCP page")
    _button(at, "Run tool").click().run()

    assert not at.exception
    assert any("Done in" in str(s.value) for s in at.success)
    assert [d["filename"] for d in rag.list_documents()] == ["From the UI.md"]

    # Missing required arguments are caught before calling the server.
    at.text_area(key="add_note_title").set_value("")
    _button(at, "Run tool").click().run()
    assert "Required: title" in _texts(at.warning)
