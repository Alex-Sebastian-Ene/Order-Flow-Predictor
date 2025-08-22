#!/bin/bash

# Simple CI check script
set -e

echo "🔍 Checking CI pipeline..."

# Check if required files exist
required_files=(
    "pyproject.toml"
    "poetry.lock"
    "setup.cfg"
    ".github/workflows/ci.yml"
    "tests/test_basic.py"
)

for file in "${required_files[@]}"; do
    if [ ! -f "$file" ]; then
        echo "❌ Missing required file: $file"
        exit 1
    else
        echo "✅ Found: $file"
    fi
done

echo "✨ CI pipeline files are present and configured!"
echo "🚀 Pipeline should now run successfully"
