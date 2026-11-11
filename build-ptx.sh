#!/bin/sh
# Convenience wrapper around the CMake build.
set -e
cd "$(dirname "$0")"
cmake -S . -B build
cmake --build build -j
echo "Built: build/ptx"
