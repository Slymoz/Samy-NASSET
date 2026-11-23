import os
import subprocess
import sys
from pathlib import Path

import pytest

from app import config

ROOT = Path(__file__).resolve().parent.parent


def test_int_setting(monkeypatch):
    monkeypatch.setenv("NEXUS_TEST_INT", "12")
    assert config._int("NEXUS_TEST_INT", 5) == 12
    monkeypatch.setenv("NEXUS_TEST_INT", "  ")
    assert config._int("NEXUS_TEST_INT", 5) == 5
    monkeypatch.delenv("NEXUS_TEST_INT")
    assert config._int("NEXUS_TEST_INT", 5) == 5


@pytest.mark.parametrize("value", ["abc", "1.5", "-1"])
def test_invalid_int_setting_is_explained(monkeypatch, value):
    monkeypatch.setenv("NEXUS_TEST_INT", value)
    with pytest.raises(ValueError, match="NEXUS_TEST_INT"):
        config._int("NEXUS_TEST_INT", 5, minimum=0)


def test_float_setting(monkeypatch):
    monkeypatch.setenv("NEXUS_TEST_FLOAT", "0.35")
    assert config._float("NEXUS_TEST_FLOAT", None) == 0.35
    monkeypatch.setenv("NEXUS_TEST_FLOAT", "nope")
    with pytest.raises(ValueError, match="NEXUS_TEST_FLOAT"):
        config._float("NEXUS_TEST_FLOAT", None)


def test_environment_overrides_are_applied():
    env = {
        **os.environ,
        "NEXUS_CHAT_MODEL": "llama3.1:8b",
        "NEXUS_OLLAMA_URL": "http://ollama:11434/",
        "NEXUS_TOP_K": "6",
        "NEXUS_MAX_DISTANCE": "0.5",
        "NEXUS_MAX_UPLOAD_MB": "2",
    }
    code = (
        "from app import config as c;"
        "print(c.CHAT_MODEL, c.OLLAMA_URL, c.TOP_K, c.MAX_DISTANCE, c.MAX_UPLOAD_BYTES)"
    )
    out = subprocess.run(
        [sys.executable, "-c", code], cwd=ROOT, env=env, capture_output=True, text=True, check=True
    ).stdout.split()
    assert out == ["llama3.1:8b", "http://ollama:11434", "6", "0.5", str(2 * 1024 * 1024)]


def test_overlap_must_be_smaller_than_chunk_size():
    env = {**os.environ, "NEXUS_CHUNK_SIZE": "100", "NEXUS_CHUNK_OVERLAP": "100"}
    result = subprocess.run(
        [sys.executable, "-c", "import app.config"],
        cwd=ROOT,
        env=env,
        capture_output=True,
        text=True,
    )
    assert result.returncode != 0
    assert "NEXUS_CHUNK_OVERLAP" in result.stderr
