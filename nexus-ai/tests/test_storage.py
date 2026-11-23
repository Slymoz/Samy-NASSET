import sqlite3

import pytest

from app import config, storage


def test_conversation_gets_title_from_first_user_message():
    cid = storage.create_conversation()
    storage.add_message(cid, "user", "  What is   RAG?  ")
    storage.add_message(cid, "assistant", "Retrieval-Augmented Generation.")
    storage.add_message(cid, "user", "Another question")  # title is kept

    titles = {c["id"]: c["title"] for c in storage.list_conversations()}
    assert titles[cid] == "What is RAG?"
    assert [m["role"] for m in storage.get_messages(cid)] == ["user", "assistant", "user"]


def test_long_first_message_gives_truncated_title():
    cid = storage.create_conversation()
    storage.add_message(cid, "user", "x" * 200)
    assert storage.get_conversation(cid)["title"] == "x" * 55


def test_invalid_role_is_rejected():
    cid = storage.create_conversation()
    with pytest.raises(ValueError):
        storage.add_message(cid, "system", "nope")


def test_message_for_unknown_conversation_is_rejected():
    with pytest.raises(ValueError):
        storage.add_message(424242, "user", "hello")


def test_delete_conversation_removes_messages():
    cid = storage.create_conversation()
    storage.add_message(cid, "user", "hello")

    assert storage.delete_conversation(cid) is True
    assert storage.conversation_exists(cid) is False
    assert storage.get_messages(cid) == []  # cascade delete
    assert storage.delete_conversation(cid) is False


def test_conversations_are_listed_by_recent_activity():
    old = storage.create_conversation()
    new = storage.create_conversation()
    storage.add_message(old, "user", "bump")
    assert [c["id"] for c in storage.list_conversations()][:2] == [old, new]


def test_rename_conversation():
    cid = storage.create_conversation()
    assert storage.rename_conversation(cid, "  My   project  ") is True
    assert storage.get_conversation(cid)["title"] == "My project"
    assert storage.rename_conversation(999999, "x") is False
    with pytest.raises(ValueError):
        storage.rename_conversation(cid, "   ")
    # A renamed conversation is not renamed again by its first message.
    storage.add_message(cid, "user", "hello")
    assert storage.get_conversation(cid)["title"] == "My project"


def test_sources_are_persisted_with_answers():
    cid = storage.create_conversation()
    sources = [
        {"document_id": "d", "filename": "f.md", "chunk": 2, "text": "t", "distance": 0.1},
    ]
    storage.add_message(cid, "user", "q")
    storage.add_message(cid, "assistant", "a", sources)

    user, assistant = storage.get_messages(cid)
    assert user["sources"] == []
    assert assistant["sources"] == sources


def test_get_messages_limit_returns_most_recent_in_order():
    cid = storage.create_conversation()
    for i in range(6):
        storage.add_message(cid, "user" if i % 2 == 0 else "assistant", f"m{i}")

    assert [m["content"] for m in storage.get_messages(cid, limit=3)] == ["m3", "m4", "m5"]
    assert storage.get_messages(cid, limit=0) == []
    assert len(storage.get_messages(cid)) == 6


def test_init_db_migrates_a_v1_database(tmp_path, monkeypatch):
    """Databases created before sources were stored keep working."""
    db_path = tmp_path / "old.db"
    monkeypatch.setattr(config, "DB_PATH", db_path)
    with sqlite3.connect(db_path) as db:
        db.executescript(
            """
            CREATE TABLE conversations (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                title TEXT NOT NULL DEFAULT 'New conversation',
                created_at TEXT NOT NULL, updated_at TEXT NOT NULL);
            CREATE TABLE messages (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                conversation_id INTEGER NOT NULL,
                role TEXT NOT NULL CHECK (role IN ('user', 'assistant')),
                content TEXT NOT NULL, created_at TEXT NOT NULL,
                FOREIGN KEY(conversation_id) REFERENCES conversations(id) ON DELETE CASCADE);
            INSERT INTO conversations VALUES (1, 'Old chat', '2025-01-01', '2025-01-01');
            INSERT INTO messages VALUES (1, 1, 'user', 'old question', '2025-01-01');
            """
        )
    db.close()

    storage.init_db()
    storage.init_db()  # idempotent

    assert storage.get_messages(1)[0]["content"] == "old question"
    assert storage.get_messages(1)[0]["sources"] == []
    storage.add_message(1, "assistant", "new answer", [{"filename": "a.md", "chunk": 0}])
    assert storage.get_messages(1)[-1]["sources"] == [{"filename": "a.md", "chunk": 0}]


def test_database_uses_wal_mode():
    with storage.connect() as db:
        assert db.execute("PRAGMA journal_mode").fetchone()[0] == "wal"
