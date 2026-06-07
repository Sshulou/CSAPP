#!/usr/bin/env bash
set -euo pipefail

git config --global --add safe.directory "$PWD" 2>/dev/null || true

echo "CSAPP toolchain ready"
gcc --version | head -n 1
gdb --version | head -n 1
make --version | head -n 1
python3 --version
perl --version | head -n 2
