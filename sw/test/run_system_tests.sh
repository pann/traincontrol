#!/usr/bin/env bash
# Run automated system tests via pytest-embedded.
# Usage: ./run_system_tests.sh [port]
#   port defaults to /dev/ttyACM0
# Prerequisites: pip install pytest-embedded pytest-embedded-serial-esp pytest-embedded-idf
set -e

# Source ESP-IDF environment if idf.py is not already on PATH
if ! command -v idf.py &>/dev/null; then
    IDF_PATH="${IDF_PATH:-$HOME/.espressif/v5.5.2/esp-idf}"
    if [ -f "$IDF_PATH/export.sh" ]; then
        . "$IDF_PATH/export.sh" >/dev/null 2>&1
    else
        echo "Error: ESP-IDF not found. Set IDF_PATH or install ESP-IDF." >&2
        exit 1
    fi
fi

if ! command -v pytest &>/dev/null; then
    echo "Error: pytest not found. Install with:" >&2
    echo "  pip install pytest-embedded pytest-embedded-serial-esp pytest-embedded-idf" >&2
    exit 1
fi

cd "$(dirname "$0")/system"
pytest --target esp32s3 -p "${1:-/dev/ttyACM0}" pytest_system.py -v
