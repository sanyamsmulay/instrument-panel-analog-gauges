# Build Tools

This directory contains the build system implementation for the Instrument Panel project.

## Files

### Build System
- `Containerfile.linux`: Defines the Linux build environment with all required dependencies
- `Containerfile.windows`: Defines the Windows build environment with all required dependencies
- `container-build.sh`: Internal script that handles the actual build process for different architectures

### Run Scripts
- `scripts/build-and-run-x86.sh`: Builds and runs the application directly on a Linux x86_64 desktop
- `scripts/run-in-container.sh`: Runs the pre-built application in a container with X11 forwarding

## Usage

From the *project root directory*, run:

### Quick Start

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

## Running the Application

There are two ways to run the application on a Linux desktop:

1. Direct execution (requires local dependencies):
```bash
./build-tools/scripts/build-and-run-x86.sh
```

2. Container execution (recommended, no local dependencies needed):
```bash
./build-tools/scripts/run-in-container.sh
```

The container execution method provides a consistent runtime environment and includes all necessary dependencies.

## Requirements

### Build Requirements
- Podman installed on the host system
- Internet connection (for first build to download base image and packages)

### Runtime Requirements
For direct execution:
- liballegro5.2 and related packages (image, font, ttf)
- libgpiod (for hardware interface)
- OpenGL/Mesa drivers

For container execution:
- Podman
- X11 server with container access allowed

## Testing

### UDP Packet Monitor

A Python test script is included to help debug network communication issues:

```bash
# Monitor UDP packets on the default port (52021)
python3 build-tools/tests/udp_packet_monitor.py

# Monitor on a specific IP and port
python3 build-tools/tests/udp_packet_monitor.py --ip 0.0.0.0 --port 52021
```

This script listens for UDP packets and displays:
- Timestamp of received packets
- Source address and port
- Packet size
- Packet contents (decoded as text or hex if binary)

Use this tool to verify that the instrument panel is sending data requests and to troubleshoot network connectivity issues.
