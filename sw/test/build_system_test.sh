#!/usr/bin/env bash
# Build the system test firmware.
set -e

# Source ESP-IDF environment if idf.py is not already on PATH
if ! command -v idf.py &>/dev/null; then
    IDF_PATH="${IDF_PATH:-$HOME/.espressif/v5.5.2/esp-idf}"
    if [ -f "$IDF_PATH/export.sh" ]; then
        echo "Sourcing ESP-IDF from $IDF_PATH ..."
        . "$IDF_PATH/export.sh"
    else
        echo "Error: ESP-IDF not found at $IDF_PATH. Set IDF_PATH or install ESP-IDF." >&2
        exit 1
    fi
fi

echo ""
echo "=== Building system test firmware ==="
cd "$(dirname "$0")/system"
echo "Build directory: $(pwd)"
echo ""

idf.py build

echo ""
echo "=== Build complete ==="
echo "Flash with: ./flash_system_test.sh [port]"
