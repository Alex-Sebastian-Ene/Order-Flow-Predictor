"""Pytest configuration and fixtures."""

import sys
from pathlib import Path

# Add the project root and src directories to Python path for all tests
# Use absolute paths to ensure CI compatibility
project_root = Path(__file__).parent.parent.absolute()
src_path = project_root / "src"
src_python_path = project_root / "src" / "python"

# Insert paths in order of priority, using absolute paths
paths_to_add = [str(project_root), str(src_path), str(src_python_path)]

for path in paths_to_add:
    if path not in sys.path:
        sys.path.insert(0, path)
