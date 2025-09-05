#!/bin/bash
set -e
set -x

# Get absolute path to project root
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Function to build Linux container image
build_linux_container() {
    if ! podman image exists instrument-panel-linux-builder; then
        echo "Building Linux container image..."
        podman build -t instrument-panel-linux-builder \
            -f "$PROJECT_ROOT/build-tools/Containerfile.linux" "$PROJECT_ROOT/build-tools"
    else
        echo "Linux container image already exists."
    fi
}

# Function to build Windows container image
build_windows_container() {
    if ! podman image exists instrument-panel-windows-builder; then
        echo "Building Windows container image..."
        podman build -t instrument-panel-windows-builder \
            -f "$PROJECT_ROOT/build-tools/Containerfile.windows" "$PROJECT_ROOT/build-tools"
    else
        echo "Windows container image already exists."
    fi
}

# Function to build for Linux architecture
build_linux_arch() {
    local arch=$1
    echo "Building for Linux $arch..."
    
    # Create output directory
    mkdir -p "$PROJECT_ROOT/build/$arch"
    
    # Run the build in container
    podman run --rm \
        -v "$PROJECT_ROOT/instrument-panel:/build/src:ro" \
        -v "$PROJECT_ROOT/build/$arch:/build/output/$arch" \
        -v "$PROJECT_ROOT/build-tools/container-build.sh:/build/container-build.sh:ro" \
        -e TARGET_OS=linux \
        -e TARGET_ARCH=$arch \
        instrument-panel-linux-builder
}

# Function to build for Windows
build_windows() {
    echo "Building for Windows..."
    
    # Create output directory
    mkdir -p "$PROJECT_ROOT/build/windows"
    
    # Run the build in container
    podman run --rm \
        -v "$PROJECT_ROOT/instrument-panel:/build/src:ro" \
        -v "$PROJECT_ROOT/build/windows:/build/output/windows" \
        -v "$PROJECT_ROOT/build-tools/container-build.sh:/build/container-build.sh:ro" \
        -e TARGET_OS=windows \
        -e TARGET_ARCH=win64 \
        instrument-panel-windows-builder
}

# Parse command line arguments
TARGET="$1"
shift

case "$TARGET" in
    "linux")
        build_linux_container
        if [ "$1" = "" ] || [ "$1" = "all" ]; then
            build_linux_arch "x86_64"
            build_linux_arch "arm64"
            build_linux_arch "armhf"
            echo "Linux build complete! Check build/* directories for outputs."
        else
            build_linux_arch "$1"
        fi
        ;;
    "windows")
        build_windows_container
        build_windows
        echo "Windows build complete! Check build/windows directory for output."
        ;;
    "all")
        # Build both Linux and Windows
        build_linux_container
        build_linux_arch "x86_64"
        build_linux_arch "arm64"
        build_linux_arch "armhf"
        
        build_windows_container
        build_windows
        
        echo "All builds complete! Check build/* directories for outputs."
        ;;
    *)
        echo "Usage: $0 [linux [arch]|windows|all]"
        echo "  linux [arch]: Build Linux version (arch: x86_64, arm64, armhf, or all)"
        echo "  windows: Build Windows version"
        echo "  all: Build all versions"
        exit 1
        ;;
esac

echo "Build complete! Check the build/* directories for outputs."
