#!/usr/bin/env bash
# Flash system test firmware to devkit and open serial monitor.
# Usage: ./flash_system_test.sh [port]
#   port defaults to /dev/ttyACM0
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

cd "$(dirname "$0")/system"
idf.py -p "${1:-/dev/ttyACM0}" flash monitor
