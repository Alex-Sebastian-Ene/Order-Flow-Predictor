"""Pytest configuration and fixtures."""

import sys
from pathlib import Path

# Add the project root and src directories to Python path for all tests
project_root = Path(__file__).parent.parent
src_path = project_root / "src"
src_python_path = project_root / "src" / "python"

# Insert paths in order of priority
sys.path.insert(0, str(project_root))
sys.path.insert(0, str(src_path))
sys.path.insert(0, str(src_python_path))
