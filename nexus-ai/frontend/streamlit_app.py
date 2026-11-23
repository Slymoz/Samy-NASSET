"""Streamlit front-end for Nexus AI.

Talks to the FastAPI backend only (never to Ollama directly), so the UI stays
a thin layer: chat with streaming, conversation history and document upload.
"""

import contextlib
import json
import os

import httpx
import streamlit as st

API_URL = os.getenv("NEXUS_API_URL", "http://127.0.0.1:8000").rstrip("/")

st.set_page_config(page_title="Nexus AI", page_icon="✳️", layout="wide")

STYLE = """
<style>
.stApp {
    background: radial-gradient(ellipse at top, #202447 0%, #11131d 55%);
    color: #f3f4f8;
}
.block-container { max-width: 1100px; padding-top: 2rem; padding-bottom: 5rem; }
.hero { text-align: center; padding: 1rem 0 1.5rem; }
.hero h1 { color: white; font-size: 2.5rem; letter-spacing: -1px; }
.hero p { color: #a5abc2; }
[data-testid="stChatMessage"] {
    background: rgba(255,255,255,.04);
    border: 1px solid rgba(255,255,255,.08);
    border-radius: 16px;
}
</style>
<div class="hero">
  <h1>✳ Nexus AI</h1>
  <p>Personal assistant · Local LLM · RAG · MCP</p>
</div>
"""


# --------------------------------------------------------------------------- #
# API helpers
# --------------------------------------------------------------------------- #
def api_get(path: str, **kwargs):
    response = httpx.get(f"{API_URL}{path}", timeout=10, **kwargs)
    response.raise_for_status()
    return response.json()


def api_request(method: str, path: str, timeout: float = 30, **kwargs):
    response = httpx.request(method, f"{API_URL}{path}", timeout=timeout, **kwargs)
    response.raise_for_status()
    return response.json()


def http_error_message(exc: httpx.HTTPError) -> str:
    """Return the backend's error detail when there is one."""
    response = getattr(exc, "response", None)
    if response is None:
        return f"Network error: {exc}"
    try:
        detail = response.json().get("detail", response.text)
    except ValueError:
        detail = response.text
    return f"HTTP error {response.status_code}: {detail}"


def show_http_error(exc: httpx.HTTPError) -> None:
    st.error(http_error_message(exc))


# --------------------------------------------------------------------------- #
# Sidebar
# --------------------------------------------------------------------------- #
def render_services(status: dict, api_ok: bool) -> None:
    st.subheader("Services")
    st.write(f"{'🟢' if api_ok else '🔴'} FastAPI")
    st.write(f"{'🟢' if status.get('ollama') else '🔴'} Ollama")
    model = status.get("model_name", "Chat model")
    st.write(f"{'🟢' if status.get('model') else '🟠'} {model}")
    embed = status.get("embedding_model_name", "Embedding model")
    st.write(f"{'🟢' if status.get('embedding_model') else '🟠'} {embed}")
    st.write(f"{'🟢' if status.get('mcp') else '🔴'} MCP server")
    if status.get("ollama") and not status.get("model"):
        st.caption(f"Run `ollama pull {model}` to enable chat.")
    if st.button("Refresh status", width="stretch"):
        st.rerun()


def render_conversation_tools(conversation_id: int, title: str) -> None:
    """Rename, export and delete the selected conversation."""
    with st.expander("Manage conversation"):
        new_title = st.text_input("Title", value=title, key=f"title_{conversation_id}")
        if st.button("Rename", width="stretch") and new_title.strip() != title:
            try:
                api_request(
                    "PATCH", f"/api/conversations/{conversation_id}", json={"title": new_title}
                )
                st.rerun()
            except httpx.HTTPError as exc:
                show_http_error(exc)

        try:
            export_url = f"{API_URL}/api/conversations/{conversation_id}/export"
            response = httpx.get(export_url, timeout=10)
            response.raise_for_status()
            st.download_button(
                "Export as Markdown",
                data=response.text,
                file_name=f"nexus-conversation-{conversation_id}.md",
                mime="text/markdown",
                width="stretch",
            )
        except httpx.HTTPError:
            st.caption("Export unavailable.")

        if st.button("Delete this conversation", width="stretch"):
            try:
                api_request("DELETE", f"/api/conversations/{conversation_id}")
                st.session_state.conversation_id = None
                st.rerun()
            except httpx.HTTPError as exc:
                show_http_error(exc)


def render_conversations() -> None:
    st.subheader("Conversations")

    try:
        conversations = api_get("/api/conversations")
    except httpx.HTTPError:
        conversations = []

    if st.button("＋ New conversation", width="stretch"):
        try:
            created = api_request("POST", "/api/conversations")
            st.session_state.conversation_id = created["id"]
            st.rerun()
        except httpx.HTTPError as exc:
            show_http_error(exc)

    if not conversations:
        st.session_state.conversation_id = None
        st.caption("No conversation yet.")
        return

    ids = [c["id"] for c in conversations]
    titles = {c["id"]: c["title"] for c in conversations}
    current = st.session_state.get("conversation_id")
    if current not in ids:
        current = ids[0]

    st.session_state.conversation_id = st.selectbox(
        "History",
        options=ids,
        index=ids.index(current),
        format_func=lambda cid: f"{titles[cid]} · #{cid}",
        label_visibility="collapsed",
    )
    selected = st.session_state.conversation_id
    render_conversation_tools(selected, titles[selected])


def upload_files(files: list) -> None:
    """Send each file to the API and report the result per file."""
    for uploaded in files:
        try:
            with st.spinner(f"Indexing {uploaded.name}..."):
                result = api_request(
                    "POST",
                    "/api/documents",
                    files={
                        "file": (
                            uploaded.name,
                            uploaded.getvalue(),
                            uploaded.type or "application/octet-stream",
                        )
                    },
                    timeout=600,
                )
            if result.get("duplicate"):
                st.info(f"{uploaded.name} is already indexed as {result['filename']}.")
            else:
                st.success(f"{result['filename']} indexed: {result['chunks']} passages.")
        except httpx.HTTPError as exc:
            st.error(f"{uploaded.name}: {http_error_message(exc)}")


def render_documents() -> None:
    st.subheader("RAG documents")

    st.session_state.use_documents = st.toggle(
        "Answer from my documents",
        value=st.session_state.get("use_documents", True),
        help="When off, Nexus answers without searching your documents.",
    )

    uploaded = st.file_uploader(
        "Import PDF, TXT or Markdown files",
        type=["pdf", "txt", "md"],
        accept_multiple_files=True,
    )

    if st.button("Index documents", width="stretch"):
        if not uploaded:
            st.warning("Choose a file first.")
        else:
            upload_files(uploaded)

    try:
        documents = api_get("/api/documents")
    except httpx.HTTPError:
        st.caption("Documents unavailable.")
        return

    if not documents:
        st.caption("No document indexed.")
        return

    st.caption(f"{len(documents)} document(s), {sum(d['chunks'] for d in documents)} passages")
    for doc in documents:
        name_col, delete_col = st.columns([5, 1])
        name_col.caption(f"📄 {doc['filename']} · {doc['chunks']}")
        if delete_col.button("🗑", key=f"del_{doc['document_id']}", help="Delete"):
            try:
                api_request("DELETE", f"/api/documents/{doc['document_id']}")
                st.rerun()
            except httpx.HTTPError as exc:
                show_http_error(exc)


# --------------------------------------------------------------------------- #
# Chat
# --------------------------------------------------------------------------- #
def render_sources(sources: list[dict]) -> None:
    with st.expander(f"📚 Retrieved sources ({len(sources)})"):
        for index, source in enumerate(sources, 1):
            st.markdown(f"**{index}. {source['filename']}** — passage {source['chunk'] + 1}")
            if source.get("text"):
                st.caption(source["text"])
            if source.get("distance") is not None:
                st.caption(f"Cosine distance: {source['distance']:.4f}")


def format_stats(stats: dict) -> str:
    parts = []
    if stats.get("elapsed_seconds") is not None:
        parts.append(f"⏱️ {stats['elapsed_seconds']} s")
    if stats.get("generated_tokens") is not None:
        parts.append(f"🔤 {stats['generated_tokens']} tokens")
    if stats.get("tokens_per_second") is not None:
        parts.append(f"⚡ {stats['tokens_per_second']} tokens/s")
    return " · ".join(parts) or "Generation finished."


def stream_answer(prompt: str, conversation_id: int, use_documents: bool = True) -> None:
    """Send the prompt and render the streamed answer as it arrives."""
    response_area = st.empty()
    status_area = st.empty()
    answer, sources, stats, error = "", [], {}, None

    status_area.caption("⏳ Nexus is preparing the answer...")

    try:
        with (
            httpx.Client(timeout=httpx.Timeout(600, connect=10)) as client,
            client.stream(
                "POST",
                f"{API_URL}/api/chat/stream",
                json={
                    "message": prompt,
                    "conversation_id": conversation_id,
                    "use_documents": use_documents,
                },
            ) as response,
        ):
            if response.is_error:
                body = response.read().decode("utf-8", errors="replace")
                with contextlib.suppress(ValueError):
                    body = json.loads(body).get("detail", body)
                raise RuntimeError(f"HTTP error {response.status_code}: {body}")

            for line in response.iter_lines():
                if not line:
                    continue
                event = json.loads(line)
                kind = event.get("type")

                if kind == "meta":
                    sources = event.get("sources", [])
                    status_area.caption(f"⏳ Generating with {event.get('model', 'the model')}...")
                elif kind == "token":
                    answer += event.get("text", "")
                    response_area.markdown(answer + " ▌")  # blinking cursor
                elif kind == "done":
                    stats = event
                    response_area.markdown(answer or "_Empty answer._")
                elif kind == "error":
                    error = event.get("message", "Unknown generation error.")
    except (httpx.HTTPError, ValueError, RuntimeError) as exc:
        error = str(exc)

    if error:
        status_area.error(f"Generation failed: {error}")
        if answer:
            response_area.markdown(answer)
    else:
        status_area.caption(format_stats(stats))

    if sources:
        render_sources(sources)


def main() -> None:
    st.markdown(STYLE, unsafe_allow_html=True)

    try:
        status, api_ok = api_get("/api/status"), True
    except httpx.HTTPError:
        status, api_ok = {}, False

    with st.sidebar:
        st.title("✳ Nexus AI")
        version = f" · v{status['version']}" if status.get("version") else ""
        st.caption(f"Local assistant and LLM lab{version}")
        st.divider()
        render_services(status, api_ok)
        st.divider()
        if api_ok:
            render_conversations()
            st.divider()
            render_documents()

    if not api_ok:
        st.error("FastAPI is unreachable. Run ./start.sh first.")
        st.stop()

    conversation_id = st.session_state.get("conversation_id")
    if conversation_id is None:
        st.info("Create a conversation in the sidebar to get started.")
        st.stop()

    try:
        history = api_get(f"/api/conversations/{conversation_id}/messages")
    except httpx.HTTPError as exc:
        show_http_error(exc)
        history = []

    for message in history:
        with st.chat_message(message["role"]):
            st.markdown(message["content"])
            if message.get("sources"):
                render_sources(message["sources"])

    prompt = st.chat_input("Ask Nexus something...")
    if prompt:
        with st.chat_message("user"):
            st.markdown(prompt)
        with st.chat_message("assistant"):
            stream_answer(prompt, conversation_id, st.session_state.get("use_documents", True))
        # No st.rerun() here: it would resubmit the same prompt. The backend
        # has already saved both messages, so they appear on the next run.


main()
