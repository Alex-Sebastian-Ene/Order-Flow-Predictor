#!/bin/bash
# Local CI Pipeline Runner
# This script runs the same checks as the GitHub Actions CI pipeline

echo "🚀 Running Local CI Pipeline..."

echo ""
echo "📦 Step 1: Installing dependencies..."
poetry install --no-interaction
if [ $? -ne 0 ]; then
    echo "❌ Dependency installation failed!"
    exit 1
fi

echo ""
echo "🧪 Step 2: Running tests..."
poetry run pytest tests/ -v
if [ $? -ne 0 ]; then
    echo "❌ Tests failed!"
    exit 1
fi

echo ""
echo "🎨 Step 3: Checking code formatting..."
poetry run black --check .
if [ $? -ne 0 ]; then
    echo "❌ Code formatting check failed!"
    echo "💡 Run 'poetry run black .' to fix formatting"
    exit 1
fi

echo ""
echo "📋 Step 4: Checking import sorting..."
poetry run isort --check-only .
if [ $? -ne 0 ]; then
    echo "❌ Import sorting check failed!"
    echo "💡 Run 'poetry run isort .' to fix imports"
    exit 1
fi

echo ""
echo "🔍 Step 5: Running flake8 linting..."
poetry run flake8 src/ tests/
if [ $? -ne 0 ]; then
    echo "❌ Linting failed!"
    exit 1
fi

echo ""
echo "🔎 Step 6: Running type checking..."
poetry run mypy src/ --config-file pyproject.toml
if [ $? -ne 0 ]; then
    echo "❌ Type checking failed!"
    exit 1
fi

echo ""
echo "✅ All CI checks passed! Your code is ready to push! 🎉"
