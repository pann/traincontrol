#!/usr/bin/env bash
# Build and run all unit tests (host-side, no hardware needed).
set -e
cd "$(dirname "$0")/unit"
cmake -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
