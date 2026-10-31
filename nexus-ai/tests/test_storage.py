from app import storage


def setup_module():
    storage.init_db()


def test_conversation_gets_title_from_first_user_message():
    cid = storage.create_conversation()
    storage.add_message(cid, "user", "  What is   RAG?  ")
    storage.add_message(cid, "assistant", "Retrieval-Augmented Generation.")

    titles = {c["id"]: c["title"] for c in storage.list_conversations()}
    assert titles[cid] == "What is RAG?"
    assert [m["role"] for m in storage.get_messages(cid)] == ["user", "assistant"]


def test_invalid_role_is_rejected():
    cid = storage.create_conversation()
    try:
        storage.add_message(cid, "system", "nope")
    except ValueError:
        return
    raise AssertionError("expected ValueError")


def test_delete_conversation_removes_messages():
    cid = storage.create_conversation()
    storage.add_message(cid, "user", "hello")

    assert storage.delete_conversation(cid) is True
    assert storage.conversation_exists(cid) is False
    assert storage.get_messages(cid) == []  # cascade delete
    assert storage.delete_conversation(cid) is False
