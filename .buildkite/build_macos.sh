#!/bin/bash
set -euo pipefail

echo "--- :package: Installing dependencies"
if command -v brew >/dev/null; then
    brew install curl json-c pkg-config || true
else
    echo "Warning: Homebrew not found. Assuming dependencies are installed."
fi

echo "--- :hammer: Building"
make clean
make

echo "--- :test_tube: Testing"
make test
