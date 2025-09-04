#!/bin/bash

# Get absolute path to project root
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Build container image if needed
if ! podman image exists instrument-panel-linux-builder; then
    echo "Building container image..."
    podman build -t instrument-panel-linux-builder -f "$PROJECT_ROOT/build-tools-linux/Containerfile" "$PROJECT_ROOT/build-tools-linux"
fi

# Function to build for a specific architecture
build_arch() {
    local arch=$1
    echo "Building for $arch..."
    
    # Create output directory
    mkdir -p "$PROJECT_ROOT/build/$arch"
    
    # Run the build in container
    podman run --rm \
        -v "$PROJECT_ROOT/instrument-panel:/build/src:ro" \
        -v "$PROJECT_ROOT/build/$arch:/build/output/$arch" \
        -e TARGET_ARCH=$arch \
        instrument-panel-linux-builder
}

# Build for all architectures or specific one
if [ "$1" = "" ] || [ "$1" = "all" ]; then
    build_arch "x86_64"
    build_arch "arm64"
    build_arch "armhf"
    echo "Build complete! Check the build/* directories for outputs."
else
    build_arch "$1"
fi
