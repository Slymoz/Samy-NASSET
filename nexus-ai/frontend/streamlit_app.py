"""Streamlit front-end for Nexus AI.

Talks to the FastAPI backend only (never to Ollama directly), so the UI stays
a thin layer: chat with streaming, conversation history and document upload.
"""

import json
import os

import httpx
import streamlit as st

API_URL = os.getenv("NEXUS_API_URL", "http://127.0.0.1:8000")

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
def api_get(path: str):
    response = httpx.get(f"{API_URL}{path}", timeout=10)
    response.raise_for_status()
    return response.json()


def api_request(method: str, path: str, **kwargs):
    response = httpx.request(method, f"{API_URL}{path}", timeout=30, **kwargs)
    response.raise_for_status()
    return response.json()


def show_http_error(exc: httpx.HTTPError) -> None:
    """Display the backend's error detail when there is one."""
    response = getattr(exc, "response", None)
    if response is None:
        st.error(f"Network error: {exc}")
        return
    try:
        detail = response.json().get("detail", response.text)
    except ValueError:
        detail = response.text
    st.error(f"HTTP error {response.status_code}: {detail}")


# --------------------------------------------------------------------------- #
# Sidebar
# --------------------------------------------------------------------------- #
def render_services(status: dict, api_ok: bool) -> None:
    st.subheader("Services")
    st.write(f"{'🟢' if api_ok else '🔴'} FastAPI")
    st.write(f"{'🟢' if status.get('ollama') else '🔴'} Ollama")
    model = status.get("model_name", "Chat model")
    st.write(f"{'🟢' if status.get('model') else '🟠'} {model}")
    st.write(f"{'🟢' if status.get('embedding_model') else '🟠'} Embedding model")
    st.write(f"{'🟢' if status.get('mcp') else '🔴'} MCP server")
    if st.button("Refresh status", use_container_width=True):
        st.rerun()


def render_conversations() -> None:
    st.subheader("Conversations")

    try:
        conversations = api_get("/api/conversations")
    except httpx.HTTPError:
        conversations = []

    if st.button("＋ New conversation", use_container_width=True):
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
        key="conversation_selector",
    )

    if st.button("Delete this conversation", use_container_width=True):
        try:
            api_request(
                "DELETE", f"/api/conversations/{st.session_state.conversation_id}"
            )
            st.rerun()
        except httpx.HTTPError as exc:
            show_http_error(exc)


def render_documents() -> None:
    st.subheader("RAG documents")

    uploaded = st.file_uploader(
        "Import a PDF, TXT or Markdown file", type=["pdf", "txt", "md"]
    )

    if st.button("Index document", use_container_width=True):
        if uploaded is None:
            st.warning("Choose a file first.")
        else:
            try:
                with st.spinner("Extracting and indexing..."):
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
                        timeout=240,
                    )
                st.success(
                    f"{result['filename']} indexed: {result['chunks']} passages."
                )
                st.rerun()
            except httpx.HTTPError as exc:
                show_http_error(exc)

    try:
        documents = api_get("/api/documents")
    except httpx.HTTPError:
        st.caption("Documents unavailable.")
        return

    if not documents:
        st.caption("No document indexed.")
    for doc in documents:
        name_col, delete_col = st.columns([5, 1])
        name_col.caption(f"📄 {doc['filename']}")
        if delete_col.button("🗑", key=f"del_{doc['document_id']}"):
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
            st.markdown(
                f"**{index}. {source['filename']}** — passage {source['chunk'] + 1}"
            )
            st.caption(source["text"])
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


def stream_answer(prompt: str, conversation_id: int) -> None:
    """Send the prompt and render the streamed answer as it arrives."""
    response_area = st.empty()
    status_area = st.empty()
    answer, sources, stats, error = "", [], {}, None

    status_area.caption("⏳ Nexus is preparing the answer...")

    try:
        with httpx.Client(timeout=None) as client:
            with client.stream(
                "POST",
                f"{API_URL}/api/chat/stream",
                json={"message": prompt, "conversation_id": conversation_id},
            ) as response:
                if response.is_error:
                    body = response.read().decode("utf-8", errors="replace")
                    raise RuntimeError(f"HTTP error {response.status_code}: {body}")

                for line in response.iter_lines():
                    if not line:
                        continue
                    event = json.loads(line)
                    kind = event.get("type")

                    if kind == "meta":
                        sources = event.get("sources", [])
                        status_area.caption(
                            f"⏳ Generating with {event.get('model', 'the model')}..."
                        )
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
        st.caption("Local assistant and LLM lab")
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

    prompt = st.chat_input("Ask Nexus something...")
    if prompt:
        with st.chat_message("user"):
            st.markdown(prompt)
        with st.chat_message("assistant"):
            stream_answer(prompt, conversation_id)
        # No st.rerun() here: it would resubmit the same prompt. The backend
        # has already saved both messages, so they appear on the next run.


main()
