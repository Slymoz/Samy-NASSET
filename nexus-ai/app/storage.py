"""SQLite persistence for conversations and messages."""

import sqlite3
from contextlib import contextmanager
from datetime import datetime, timezone

from app import config

DEFAULT_TITLE = "New conversation"
VALID_ROLES = ("user", "assistant")


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
    """Create the schema (idempotent)."""
    with connect() as db:
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


def create_conversation(title: str = DEFAULT_TITLE) -> int:
    timestamp = _now()
    with connect() as db:
        cursor = db.execute(
            "INSERT INTO conversations (title, created_at, updated_at) "
            "VALUES (?, ?, ?)",
            (title, timestamp, timestamp),
        )
        return cursor.lastrowid


def conversation_exists(conversation_id: int) -> bool:
    with connect() as db:
        row = db.execute(
            "SELECT 1 FROM conversations WHERE id = ?", (conversation_id,)
        ).fetchone()
    return row is not None


def list_conversations() -> list[dict]:
    """Return all conversations, most recently active first."""
    with connect() as db:
        rows = db.execute(
            "SELECT id, title, created_at, updated_at FROM conversations "
            "ORDER BY updated_at DESC, id DESC"
        ).fetchall()
    return [dict(row) for row in rows]


def delete_conversation(conversation_id: int) -> bool:
    """Delete a conversation and its messages. Returns False if unknown."""
    with connect() as db:
        cursor = db.execute(
            "DELETE FROM conversations WHERE id = ?", (conversation_id,)
        )
        return cursor.rowcount > 0


def add_message(conversation_id: int, role: str, content: str) -> None:
    """Append a message and bump the conversation's ``updated_at``.

    A conversation that still has the default title is named after its
    first user message.
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
            "INSERT INTO messages (conversation_id, role, content, created_at) "
            "VALUES (?, ?, ?, ?)",
            (conversation_id, role, content, timestamp),
        )

        title = row["title"]
        if role == "user" and title == DEFAULT_TITLE:
            title = " ".join(content.split())[:55] or title

        db.execute(
            "UPDATE conversations SET title = ?, updated_at = ? WHERE id = ?",
            (title, timestamp, conversation_id),
        )


def get_messages(conversation_id: int) -> list[dict]:
    """Return a conversation's messages in chronological order."""
    with connect() as db:
        rows = db.execute(
            "SELECT id, conversation_id, role, content, created_at "
            "FROM messages WHERE conversation_id = ? ORDER BY id",
            (conversation_id,),
        ).fetchall()
    return [dict(row) for row in rows]
