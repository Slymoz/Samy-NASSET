"""SQLite persistence for conversations and messages."""

import json
import sqlite3
from contextlib import contextmanager
from datetime import datetime, timezone

from app import config

DEFAULT_TITLE = "New conversation"
VALID_ROLES = ("user", "assistant")
TITLE_MAX_LENGTH = 80


@contextmanager
def connect():
    """Open a connection, commit on success, roll back on error, always close.

    (A bare ``with sqlite3.connect()`` only manages the transaction and never
    closes the connection, hence this small wrapper.)
    """
    config.DB_PATH.parent.mkdir(parents=True, exist_ok=True)
    db = sqlite3.connect(config.DB_PATH, timeout=10)
    db.row_factory = sqlite3.Row
    db.execute("PRAGMA foreign_keys = ON")  # needed for ON DELETE CASCADE
    try:
        yield db
        db.commit()
    except Exception:
        db.rollback()
        raise
    finally:
        db.close()


def _now() -> str:
    return datetime.now(timezone.utc).isoformat()


def init_db() -> None:
    """Create the schema and apply migrations (idempotent)."""
    with connect() as db:
        # WAL lets the API and the MCP server read while the other writes.
        db.execute("PRAGMA journal_mode = WAL")
        db.executescript(
            f"""
            CREATE TABLE IF NOT EXISTS conversations (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                title TEXT NOT NULL DEFAULT '{DEFAULT_TITLE}',
                created_at TEXT NOT NULL,
                updated_at TEXT NOT NULL
            );

            CREATE TABLE IF NOT EXISTS messages (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                conversation_id INTEGER NOT NULL,
                role TEXT NOT NULL CHECK (role IN ('user', 'assistant')),
                content TEXT NOT NULL,
                created_at TEXT NOT NULL,
                FOREIGN KEY(conversation_id)
                    REFERENCES conversations(id)
                    ON DELETE CASCADE
            );

            CREATE INDEX IF NOT EXISTS idx_messages_conversation
            ON messages(conversation_id, id);
            """
        )

        # v2: sources used for an answer, stored as JSON (NULL for old rows).
        columns = {row["name"] for row in db.execute("PRAGMA table_info(messages)")}
        if "sources" not in columns:
            db.execute("ALTER TABLE messages ADD COLUMN sources TEXT")


def create_conversation(title: str = DEFAULT_TITLE) -> int:
    timestamp = _now()
    with connect() as db:
        cursor = db.execute(
            "INSERT INTO conversations (title, created_at, updated_at) VALUES (?, ?, ?)",
            (title, timestamp, timestamp),
        )
        return cursor.lastrowid


def get_conversation(conversation_id: int) -> dict | None:
    """Return a conversation's metadata, or None if it does not exist."""
    with connect() as db:
        row = db.execute(
            "SELECT id, title, created_at, updated_at FROM conversations WHERE id = ?",
            (conversation_id,),
        ).fetchone()
    return dict(row) if row else None


def conversation_exists(conversation_id: int) -> bool:
    return get_conversation(conversation_id) is not None


def list_conversations() -> list[dict]:
    """Return all conversations, most recently active first."""
    with connect() as db:
        rows = db.execute(
            "SELECT id, title, created_at, updated_at FROM conversations "
            "ORDER BY updated_at DESC, id DESC"
        ).fetchall()
    return [dict(row) for row in rows]


def rename_conversation(conversation_id: int, title: str) -> bool:
    """Rename a conversation. Returns False if unknown."""
    title = " ".join(title.split())[:TITLE_MAX_LENGTH]
    if not title:
        raise ValueError("The title cannot be empty.")
    with connect() as db:
        cursor = db.execute(
            "UPDATE conversations SET title = ? WHERE id = ?", (title, conversation_id)
        )
        return cursor.rowcount > 0


def delete_conversation(conversation_id: int) -> bool:
    """Delete a conversation and its messages. Returns False if unknown."""
    with connect() as db:
        cursor = db.execute("DELETE FROM conversations WHERE id = ?", (conversation_id,))
        return cursor.rowcount > 0


def _serialize_sources(sources: list[dict] | None) -> str | None:
    """Serialise the passages an answer was based on (JSON)."""
    if not sources:
        return None
    keep = ("document_id", "filename", "chunk", "text", "distance")
    return json.dumps([{k: s[k] for k in keep if k in s} for s in sources])


def add_message(
    conversation_id: int, role: str, content: str, sources: list[dict] | None = None
) -> None:
    """Append a message and bump the conversation's ``updated_at``.

    A conversation that still has the default title is named after its
    first user message. ``sources`` (retrieved passages) are kept for
    assistant messages so the UI can show them again later.
    """
    if role not in VALID_ROLES:
        raise ValueError(f"Invalid message role: {role!r}")

    timestamp = _now()
    with connect() as db:
        row = db.execute(
            "SELECT title FROM conversations WHERE id = ?", (conversation_id,)
        ).fetchone()
        if row is None:
            raise ValueError(f"Conversation {conversation_id} does not exist.")

        db.execute(
            "INSERT INTO messages (conversation_id, role, content, created_at, sources) "
            "VALUES (?, ?, ?, ?, ?)",
            (conversation_id, role, content, timestamp, _serialize_sources(sources)),
        )

        title = row["title"]
        if role == "user" and title == DEFAULT_TITLE:
            title = " ".join(content.split())[:55] or title

        db.execute(
            "UPDATE conversations SET title = ?, updated_at = ? WHERE id = ?",
            (title, timestamp, conversation_id),
        )


def get_messages(conversation_id: int, limit: int | None = None) -> list[dict]:
    """Return a conversation's messages in chronological order.

    With ``limit``, only the most recent ``limit`` messages are returned.
    """
    query = (
        "SELECT id, conversation_id, role, content, created_at, sources "
        "FROM messages WHERE conversation_id = ? ORDER BY id DESC"
    )
    params: tuple = (conversation_id,)
    if limit is not None:
        query += " LIMIT ?"
        params += (max(0, limit),)

    with connect() as db:
        rows = db.execute(query, params).fetchall()

    messages = []
    for row in reversed(rows):
        message = dict(row)
        message["sources"] = json.loads(message["sources"]) if message["sources"] else []
        messages.append(message)
    return messages
