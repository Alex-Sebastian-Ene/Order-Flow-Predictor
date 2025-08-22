# Use a specific gcc version for better caching
FROM gcc:13 AS builder

# Set environment variables
ENV DEBIAN_FRONTEND=noninteractive \
    PYTHONUNBUFFERED=1 \
    PYTHONDONTWRITEBYTECODE=1 \
    PIP_NO_CACHE_DIR=1

# Install dependencies with cache mount
RUN --mount=type=cache,target=/var/cache/apt \
    apt-get update && apt-get install -y \
    cmake \
    ninja-build \
    python3-full \
    python3-pip \
    python3-venv \
    git \
    ccache

# Set up ccache
ENV CCACHE_DIR=/ccache
RUN mkdir -p /ccache

# Create and activate virtual environment
ENV VIRTUAL_ENV=/opt/venv
RUN python3 -m venv $VIRTUAL_ENV
ENV PATH="$VIRTUAL_ENV/bin:$PATH"

# Install conan with pip caching
RUN --mount=type=cache,target=/root/.cache/pip \
    python3 -m pip install --upgrade pip && \
    python3 -m pip install conan

# Set work directory and copy only necessary files
WORKDIR /app
COPY CMakeLists.txt .
COPY src/cpp src/cpp/

# Set up conan profile with optimized settings
RUN conan profile detect && \
    echo "compiler.libcxx=libstdc++11" >> ~/.conan2/profiles/default && \
    echo "[conf]" >> ~/.conan2/profiles/default && \
    echo "tools.compilation:ccache=True" >> ~/.conan2/profiles/default

# Enable ccache for faster rebuilds
ENV CC="ccache gcc" \
    CXX="ccache g++"

# Command to build with optimized settings
CMD ["/bin/bash"]
