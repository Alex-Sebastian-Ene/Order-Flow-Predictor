# PowerShell script to create labels for the repository
# Run this if the GitHub Actions labeler doesn't have permission to create labels

$REPO = "Alex-Sebastian-Ene/Order-Flow-Predictor"

# Check if gh CLI is available
if (-not (Get-Command gh -ErrorAction SilentlyContinue)) {
    Write-Host "GitHub CLI (gh) is required but not installed." -ForegroundColor Red
    Write-Host "Please install it from: https://cli.github.com/" -ForegroundColor Yellow
    exit 1
}

Write-Host "Creating labels for repository: $REPO" -ForegroundColor Green

# Language labels
try { gh label create "cpp" --color "0052cc" --description "C++ related changes" --repo $REPO } catch { Write-Host "Label 'cpp' already exists" }
try { gh label create "python" --color "3776ab" --description "Python related changes" --repo $REPO } catch { Write-Host "Label 'python' already exists" }

# Component labels
try { gh label create "model" --color "8e44ad" --description "Machine learning model changes" --repo $REPO } catch { Write-Host "Label 'model' already exists" }
try { gh label create "data" --color "f39c12" --description "Data processing changes" --repo $REPO } catch { Write-Host "Label 'data' already exists" }
try { gh label create "tests" --color "28a745" --description "Test related changes" --repo $REPO } catch { Write-Host "Label 'tests' already exists" }
try { gh label create "documentation" --color "007bff" --description "Documentation changes" --repo $REPO } catch { Write-Host "Label 'documentation' already exists" }

# Type labels
try { gh label create "feature" --color "a2eeef" --description "New feature or enhancement" --repo $REPO } catch { Write-Host "Label 'feature' already exists" }
try { gh label create "bug" --color "d73a4a" --description "Bug fix" --repo $REPO } catch { Write-Host "Label 'bug' already exists" }
try { gh label create "version" --color "fef2c0" --description "Version related changes" --repo $REPO } catch { Write-Host "Label 'version' already exists" }
try { gh label create "dependencies" --color "0366d6" --description "Dependencies changes" --repo $REPO } catch { Write-Host "Label 'dependencies' already exists" }

# Size labels
try { gh label create "size/xs" --color "c2e0c6" --description "Extra small PR (1-10 lines)" --repo $REPO } catch { Write-Host "Label 'size/xs' already exists" }
try { gh label create "size/s" --color "7fcdcd" --description "Small PR (11-30 lines)" --repo $REPO } catch { Write-Host "Label 'size/s' already exists" }
try { gh label create "size/m" --color "ffeb3b" --description "Medium PR (31-100 lines)" --repo $REPO } catch { Write-Host "Label 'size/m' already exists" }
try { gh label create "size/l" --color "ff9800" --description "Large PR (101-500 lines)" --repo $REPO } catch { Write-Host "Label 'size/l' already exists" }
try { gh label create "size/xl" --color "f44336" --description "Extra large PR (501-1000 lines)" --repo $REPO } catch { Write-Host "Label 'size/xl' already exists" }
try { gh label create "size/xxl" --color "9c27b0" --description "Extra extra large PR (1000+ lines)" --repo $REPO } catch { Write-Host "Label 'size/xxl' already exists" }

Write-Host "Label creation complete!" -ForegroundColor Green
