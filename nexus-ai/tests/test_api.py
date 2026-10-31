from fastapi.testclient import TestClient

from app.main import app


def test_api_basics():
    # Using the client as a context manager runs the lifespan (DB init).
    with TestClient(app) as client:
        assert client.get("/api/health").json()["status"] == "ok"

        created = client.post("/api/conversations").json()
        assert client.get(f"/api/conversations/{created['id']}/messages").json() == []
        assert client.get("/api/conversations/999999/messages").status_code == 404

        assert client.get("/api/documents").status_code == 200
        bad_upload = client.post("/api/documents", files={"file": ("x.exe", b"data")})
        assert bad_upload.status_code == 400

        assert client.delete(f"/api/conversations/{created['id']}").status_code == 200
