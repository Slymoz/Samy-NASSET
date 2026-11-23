# Nexus AI: API + web UI + MCP server in one image.
# Ollama runs separately (see docker-compose.yml, or point NEXUS_OLLAMA_URL
# at an Ollama already running on the host).
FROM python:3.15-slim

ENV PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1 \
    PIP_NO_CACHE_DIR=1 \
    PIP_DISABLE_PIP_VERSION_CHECK=1 \
    NEXUS_HOST=0.0.0.0 \
    NEXUS_DATA_DIR=/data

RUN apt-get update \
    && apt-get install -y --no-install-recommends curl \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --create-home --uid 1000 nexus \
    && mkdir /data && chown nexus:nexus /data

WORKDIR /app

# Same layout as a local install: start.sh expects a venv in ./.venv.
COPY requirements.txt .
RUN python -m venv .venv && .venv/bin/pip install -r requirements.txt

COPY app ./app
COPY frontend ./frontend
COPY start.sh LICENSE ./

USER nexus
VOLUME ["/data"]
EXPOSE 8000 8501 8001

HEALTHCHECK --interval=30s --timeout=5s --start-period=40s --retries=3 \
    CMD curl -sf http://127.0.0.1:8000/api/health || exit 1

CMD ["./start.sh"]
