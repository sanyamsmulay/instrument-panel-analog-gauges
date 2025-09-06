#!/bin/bash
# Runs the build output on a linux desktop system

# Get absolute path to project root
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

# Build and run
"$PROJECT_ROOT/build-tools/build.sh" linux x86_64 && "$PROJECT_ROOT/build/x86_64/instrument-panel-x86_64"
