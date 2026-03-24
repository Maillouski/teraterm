#!/bin/bash
# rebuild_macos.sh - Pull latest changes, clean build, and recompile Tera Term for macOS
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
BRANCH="claude/setup-build-environment-SSGMC"

echo "=== Tera Term macOS rebuild script ==="

# Detect architecture
ARCH=$(uname -m)
if [ "$ARCH" = "x86_64" ]; then
    ARCH="x86_64"
else
    ARCH="arm64"
fi
echo "Architecture: $ARCH"

# Pull latest changes
echo ""
echo "=== Git pull ==="
cd "$SCRIPT_DIR"
git fetch origin "$BRANCH"
git checkout "$BRANCH"
git pull origin "$BRANCH"

# Clean old build
echo ""
echo "=== Cleaning build directory ==="
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

# Configure
echo ""
echo "=== CMake configure ==="
cd "$BUILD_DIR"
cmake .. -G Ninja \
    -DARCHITECTURE="$ARCH" \
    -DCMAKE_TOOLCHAIN_FILE=../macos.toolchain.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DTTXSSH=OFF \
    -DENABLE_TTXSAMPLES=OFF

# Build
echo ""
echo "=== Building ==="
cmake --build . 2>&1 | tee build.log

if [ ${PIPESTATUS[0]} -eq 0 ]; then
    echo ""
    echo "=== Build successful ==="
    # Find the built executable
    EXEC=$(find "$BUILD_DIR" -name "teraterm" -type f -perm +111 2>/dev/null | head -1)
    if [ -n "$EXEC" ]; then
        echo "Executable: $EXEC"
        echo "Run with: $EXEC"
    fi
else
    echo ""
    echo "=== Build FAILED ==="
    echo "See build.log for details"
    exit 1
fi
