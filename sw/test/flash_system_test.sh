#!/usr/bin/env bash
# Flash system test firmware to devkit and open serial monitor.
# Usage: ./flash_system_test.sh [port]
#   port defaults to /dev/ttyACM0
set -e
cd "$(dirname "$0")/system"
idf.py -p "${1:-/dev/ttyACM0}" flash monitor
