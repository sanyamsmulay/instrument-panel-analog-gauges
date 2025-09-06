#!/bin/bash
# Runs the instrument panel application in a container with X11 forwarding
set -e
set -x

# Get absolute path to project root
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

# Ensure the container image exists
if ! podman image exists instrument-panel-linux-builder; then
    echo "Building container image first..."
    cd "$PROJECT_ROOT/build-tools"
    podman build -t instrument-panel-linux-builder -f Containerfile.linux .
    cd - > /dev/null
fi

# Get the X11 socket path and XAUTHORITY
XSOCK=/tmp/.X11-unix
XAUTH=/tmp/.docker.xauth
touch ${XAUTH}
xauth nlist $DISPLAY | sed -e 's/^..../ffff/' | xauth -f ${XAUTH} nmerge -

# Run the application in container with X11 forwarding
# TODO: make port a runtime argument
podman run --rm -it \
    --device /dev/dri \
    --security-opt label=type:container_runtime_t \
    -p 52020:52020 \
    -e DISPLAY=$DISPLAY \
    -e XAUTHORITY=${XAUTH} \
    -v ${XSOCK}:${XSOCK} \
    -v ${XAUTH}:${XAUTH} \
    -v "$PROJECT_ROOT/instrument-panel:/build/src:ro" \
    -v "$PROJECT_ROOT/build/x86_64:/build/output/x86_64" \
    -v "$PROJECT_ROOT/instrument-panel/settings:/build/output/x86_64/settings:ro" \
    -v "$PROJECT_ROOT/instrument-panel/bitmaps:/build/output/x86_64/bitmaps:ro" \
    instrument-panel-linux-builder \
    bash -c "cd /build/output/x86_64 && \
             ./instrument-panel-x86_64"
