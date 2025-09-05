# Build Tools

This directory contains the build system implementation for the Instrument Panel project.

## Files

- `Containerfile`: Defines the build environment with all required dependencies
- `container-build.sh`: Internal script that handles the actual build process for different architectures

## Usage

From the *project root directory*, run:

```bash
# Make the build script executable
chmod +x build-tools/build.sh

# Build for all architectures (Linux and Windows)
./build-tools/build.sh all

# Build for all Linux architectures
./build-tools/build.sh linux
# Build for specific Linux architecture
./build-tools/build.sh linux x86_64  # For x86_64
./build-tools/build.sh linux arm64   # For ARM64
./build-tools/build.sh linux armhf   # For ARM32

# Build for Windows
# Buils for all Windows architectures
./build-tools/build.sh windows 
# Build for specific Windows architectures
./build-tools/build.sh windows win64 # For x86_64
# TODO: ./build-tools/build.sh windows win32 # For x86_32
```

## Build Outputs

The build artifacts will be placed in the `build/` directory in the project root, organized by architecture:

```
build/
  ├── x86_64/
  │   └── instrument-panel-x86_64
  ├── arm64/
  │   └── instrument-panel-arm64
  ├── armhf/
  │   └── instrument-panel-armhf
  └── windows/
      └── instrument-panel-win64.exe
```

## Requirements

- Podman installed on the host system
- Internet connection (for first build to download base image and packages)
