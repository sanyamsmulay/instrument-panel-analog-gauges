# Linux Build Tools

This directory contains tools for building the instrument panel application for various Linux architectures using Podman containers.

## Supported Architectures

- `x86_64`: 64-bit x86 Linux systems
- `arm64`: 64-bit ARM systems (e.g., Raspberry Pi 4 with 64-bit OS)
- `armhf`: 32-bit ARM systems with hardware floating point (e.g., Raspberry Pi 3/4 with 32-bit OS)

## Directory Contents

- `Containerfile`: Defines the build environment with cross-compilers and libraries for all architectures
- `container-build.sh`: Script that runs inside the container to perform the actual build
- `build.sh`: Main script to build the container and run builds for specified architectures
- `README.md`: This file

## Prerequisites

- Podman installed and running
- Internet connection (for first-time container build)
- Sufficient disk space (~2GB for container image)

## Usage

To build for all supported architectures:
```bash
./build.sh all
```

To build for a specific architecture:
```bash
./build.sh x86_64  # For 64-bit x86
./build.sh arm64   # For 64-bit ARM
./build.sh armhf   # For 32-bit ARM
```

## Build Outputs

Built binaries will be placed in the following directories:
- `build/x86_64/instrument-panel-x86_64`
- `build/arm64/instrument-panel-arm64`
- `build/armhf/instrument-panel-armhf`

## Dependencies

The build environment includes:
- GCC/G++ and cross-compilers
- libgpiod for GPIO support
- Allegro 5 libraries for graphics
- pthread for threading support

Each architecture-specific build uses its corresponding cross-compiler and library versions.

## Notes

- The build process uses a containerized environment to ensure consistent builds across different host systems
- All necessary dependencies are installed in the container
- Source files are mounted read-only in the container for safety
- Build outputs are stored outside the container in the project's build directory
