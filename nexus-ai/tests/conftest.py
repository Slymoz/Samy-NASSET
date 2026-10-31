"""Test setup: point Nexus at a throw-away data directory.

This runs before any test module is imported, so ``app.config`` picks up the
temporary location and tests never touch real data.
"""

import os
import tempfile

os.environ["NEXUS_DATA_DIR"] = tempfile.mkdtemp(prefix="nexus-test-")
