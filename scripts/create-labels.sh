#!/bin/bash

# Script to create labels for the repository
# Run this if the GitHub Actions labeler doesn't have permission to create labels

REPO="Alex-Sebastian-Ene/Order-Flow-Predictor"

# Check if gh CLI is available
if ! command -v gh &> /dev/null; then
    echo "GitHub CLI (gh) is required but not installed."
    echo "Please install it from: https://cli.github.com/"
    exit 1
fi

echo "Creating labels for repository: $REPO"

# Language labels
gh label create "cpp" --color "0052cc" --description "C++ related changes" --repo "$REPO" || echo "Label 'cpp' already exists"
gh label create "python" --color "3776ab" --description "Python related changes" --repo "$REPO" || echo "Label 'python' already exists"

# Component labels
gh label create "model" --color "8e44ad" --description "Machine learning model changes" --repo "$REPO" || echo "Label 'model' already exists"
gh label create "data" --color "f39c12" --description "Data processing changes" --repo "$REPO" || echo "Label 'data' already exists"
gh label create "tests" --color "28a745" --description "Test related changes" --repo "$REPO" || echo "Label 'tests' already exists"
gh label create "documentation" --color "007bff" --description "Documentation changes" --repo "$REPO" || echo "Label 'documentation' already exists"

# Type labels
gh label create "feature" --color "a2eeef" --description "New feature or enhancement" --repo "$REPO" || echo "Label 'feature' already exists"
gh label create "bug" --color "d73a4a" --description "Bug fix" --repo "$REPO" || echo "Label 'bug' already exists"
gh label create "version" --color "fef2c0" --description "Version related changes" --repo "$REPO" || echo "Label 'version' already exists"
gh label create "dependencies" --color "0366d6" --description "Dependencies changes" --repo "$REPO" || echo "Label 'dependencies' already exists"

# Size labels
gh label create "size/xs" --color "c2e0c6" --description "Extra small PR (1-10 lines)" --repo "$REPO" || echo "Label 'size/xs' already exists"
gh label create "size/s" --color "7fcdcd" --description "Small PR (11-30 lines)" --repo "$REPO" || echo "Label 'size/s' already exists"
gh label create "size/m" --color "ffeb3b" --description "Medium PR (31-100 lines)" --repo "$REPO" || echo "Label 'size/m' already exists"
gh label create "size/l" --color "ff9800" --description "Large PR (101-500 lines)" --repo "$REPO" || echo "Label 'size/l' already exists"
gh label create "size/xl" --color "f44336" --description "Extra large PR (501-1000 lines)" --repo "$REPO" || echo "Label 'size/xl' already exists"
gh label create "size/xxl" --color "9c27b0" --description "Extra extra large PR (1000+ lines)" --repo "$REPO" || echo "Label 'size/xxl' already exists"

echo "Label creation complete!"
