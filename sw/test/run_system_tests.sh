#!/usr/bin/env bash
# Run automated system tests via pytest-embedded.
# Usage: ./run_system_tests.sh [port]
#   port defaults to /dev/ttyACM0
# Prerequisites: pip install pytest-embedded pytest-embedded-serial-esp pytest-embedded-idf
set -e
cd "$(dirname "$0")/system"
pytest --target esp32s3 -p "${1:-/dev/ttyACM0}" pytest_system.py -v
