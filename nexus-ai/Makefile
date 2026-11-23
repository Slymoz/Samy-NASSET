# Shortcuts for the commands documented in the README.
VENV   := .venv
BIN    := $(VENV)/bin
PYTHON ?= python3

.PHONY: help install dev run test cov lint format smoke smoke-ollama docker-build docker-up docker-down clean

help:  ## List the available targets
	@grep -E '^[a-z-]+:.*##' $(MAKEFILE_LIST) | awk -F':.*## ' '{printf "  %-14s %s\n", $$1, $$2}'

$(BIN)/activate:
	$(PYTHON) -m venv $(VENV)

install: $(BIN)/activate  ## Create the venv and install runtime dependencies
	$(BIN)/pip install -r requirements.txt

dev: $(BIN)/activate  ## Install runtime + development dependencies
	$(BIN)/pip install -r requirements-dev.txt

run:  ## Start API + UI + MCP server
	./start.sh

test:  ## Run the unit, API, MCP and UI tests (no Ollama needed)
	$(BIN)/pytest

cov:  ## Run the tests with a coverage report
	$(BIN)/pytest --cov --cov-report=term-missing

lint:  ## Lint and check formatting
	$(BIN)/ruff check .
	$(BIN)/ruff format --check .

format:  ## Fix lint issues and format the code
	$(BIN)/ruff check --fix .
	$(BIN)/ruff format .

smoke:  ## Boot the full stack and check every service (no Ollama needed)
	./scripts/smoke_test.sh

smoke-ollama:  ## Same, plus indexing, search, chat and MCP through a real Ollama
	SMOKE_WITH_OLLAMA=1 ./scripts/smoke_test.sh

docker-build:  ## Build the Docker image
	docker build -t nexus-ai:local .

docker-up:  ## Start Ollama + Nexus with Docker Compose
	docker compose up -d --build

docker-down:  ## Stop the Docker Compose stack
	docker compose down

clean:  ## Remove caches (keeps the venv and your data)
	rm -rf .pytest_cache .ruff_cache .coverage coverage.xml htmlcov
	find . -name __pycache__ -type d -not -path './.venv/*' -prune -exec rm -rf {} +
