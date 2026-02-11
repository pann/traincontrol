#!/usr/bin/env bash
# Build the system test firmware (requires ESP-IDF environment).
set -e
cd "$(dirname "$0")/system"
idf.py build
