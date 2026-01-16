#!/usr/bin/env bash
set -euo pipefail

# Simple build script (Linux). Requires raylib + ODBC dev libs installed.

SRC_FILES=$(find ./src -name "*.c" -print)

mkdir -p build

gcc $SRC_FILES -Iinclude -o build/showroom \
  -lraylib -lglfw -lGL -lm -lpthread -ldl -lrt -lodbc

echo "Build success: build/showroom"
