#!/usr/bin/env bash
set -euo pipefail

if command -v conan &> /dev/null; then
    CONAN_CMD="conan"
elif [ -x "$HOME/.venv/conan/bin/conan" ]; then
    CONAN_CMD="$HOME/.venv/conan/bin/conan"
else
    echo "Error: 'conan' is not installed or not in PATH, and no \$HOME/.venv/conan/bin/conan found."
    echo "Please install Conan (e.g. via 'pipx install conan') or adjust CONAN_CMD path in this script."
    exit 1
fi

echo "Using Conan at: $CONAN_CMD"
echo "Cleaning Conan build directory..."
rm -rf build/conan

echo "Installing dependencies with Conan..."
$CONAN_CMD install . --output-folder=build/conan --build=missing

echo "Generating proto files in raft_node..."
chmod +x raft_node/protogen.sh
./raft_node/protogen.sh

echo "Done."
