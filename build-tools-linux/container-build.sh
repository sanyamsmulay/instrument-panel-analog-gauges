#!/bin/bash

# Function to build for a specific architecture
build_for_arch() {
    local arch=$1
    echo "Building for architecture: $arch"
    
    # Set up architecture-specific compiler and flags
    case $arch in
        "arm64")
            export CXX=aarch64-linux-gnu-g++
            export CFLAGS="-march=armv8-a"
            export CXXFLAGS="-march=armv8-a"
            export LIB_PATH="/usr/lib/aarch64-linux-gnu"
            ;;
        "armhf")
            export CXX=arm-linux-gnueabihf-g++
            export CFLAGS="-march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard"
            export CXXFLAGS="-march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard"
            export LIB_PATH="/usr/lib/arm-linux-gnueabihf"
            ;;
        "x86_64")
            export CXX=g++
            export CFLAGS="-march=x86-64"
            export CXXFLAGS="-march=x86-64"
            export LIB_PATH="/usr/lib/x86_64-linux-gnu"
            ;;
        *)
            echo "Unsupported architecture: $arch"
            exit 1
            ;;
    esac

    # Create build directory
    mkdir -p /build/output/$arch
    cd /build/output/$arch

    # Core source files
    CORE_FILES=(
        simvarDefs.cpp
        simvars.cpp
        globals.cpp
        knobs.cpp
        instrument.cpp
        instrument-panel.cpp
    )
    
    # Add full path to core files
    CORE_SOURCES=""
    for file in "${CORE_FILES[@]}"; do
        CORE_SOURCES+="/build/src/$file "
    done

    # Get instrument files
    INSTRUMENT_SOURCES=""
    for file in /build/src/instruments/*.cpp; do
        INSTRUMENT_SOURCES+="$file "
    done
    for subdir in /build/src/instruments/*/; do
        for file in "$subdir"*.cpp; do
            INSTRUMENT_SOURCES+="$file "
        done
    done

    # Build the project
    $CXX $CXXFLAGS -o instrument-panel-$arch \
        -I /build/src -I /build/src/instruments \
        $CORE_SOURCES \
        $INSTRUMENT_SOURCES \
        -L$LIB_PATH \
        -lgpiod -lpthread -lallegro -lallegro_image -lallegro_font -lallegro_ttf

    echo "Build completed for $arch"
    cd /build
}

# Main execution
if [ -z "$TARGET_ARCH" ]; then
    echo "Error: TARGET_ARCH not set"
    exit 1
fi

build_for_arch "$TARGET_ARCH"
