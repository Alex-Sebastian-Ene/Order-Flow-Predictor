param (
    [string]$Job = "all"
)

# Function to run Docker commands for specific jobs
function Run-Job {
    param (
        [string]$JobName,
        [string]$DockerfileContent,
        [string]$BuildContext = "."
    )
    
    Write-Host "Running job: $JobName" -ForegroundColor Green
    
    # Create temporary Dockerfile
    $tmpDir = Join-Path $PSScriptRoot "tmp-$JobName"
    New-Item -ItemType Directory -Force -Path $tmpDir | Out-Null
    $dockerfilePath = Join-Path $tmpDir "Dockerfile"
    Set-Content -Path $dockerfilePath -Value $DockerfileContent
    
    try {
        # Build and run the Docker container
        docker build -t "ci-$JobName" -f $dockerfilePath $BuildContext
        docker run --rm "ci-$JobName"
    }
    finally {
        # Cleanup
        Remove-Item -Recurse -Force $tmpDir
    }
}

# CPP Build Job
$cppDockerfile = @"
FROM ubuntu:latest

RUN apt-get update && apt-get install -y \
    cmake \
    build-essential \
    git

WORKDIR /app
COPY . .

RUN cmake -B build -S src/cpp
RUN cmake --build build
RUN cd build && ctest -C Debug --output-on-failure
"@

# Python Build Job
$pythonDockerfile = @"
FROM python:3.11

RUN curl -sSL https://install.python-poetry.org | python3 -

WORKDIR /app
COPY . .

RUN poetry install
RUN poetry run pytest tests/
RUN poetry run black --check .
RUN poetry run pylint src/python/
"@

# Run specified job or all jobs
if ($Job -eq "all" -or $Job -eq "cpp") {
    Run-Job -JobName "cpp-build" -DockerfileContent $cppDockerfile
}

if ($Job -eq "all" -or $Job -eq "python") {
    Run-Job -JobName "python-build" -DockerfileContent $pythonDockerfile
}

# Run Docker builds if specified
if ($Job -eq "all" -or $Job -eq "docker") {
    Write-Host "Building ML Docker image" -ForegroundColor Green
    docker build -t order-flow-predictor-ml:latest -f docker/ml.Dockerfile .
    
    Write-Host "Building C++ Docker image" -ForegroundColor Green
    docker build -t order-flow-predictor-cpp:latest -f docker/cpp.Dockerfile .
}
