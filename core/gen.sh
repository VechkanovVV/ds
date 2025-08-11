#!/usr/bin/env bash
set -euo pipefail
PROTOC_VER="3.21.12"
if command -v conan &> /dev/null; then
    CONAN_CMD="conan"
elif [ -x "$HOME/.venv/conan/bin/conan" ]; then
    CONAN_CMD="$HOME/.venv/conan/bin/conan"
else
    echo "Error: 'conan' is not installed or not in PATH, and no \$HOME/.venv/conan/bin/conan found."
    exit 1
fi
rm -rf build/conan
$CONAN_CMD install . --output-folder=build/conan --build=missing

PLUGIN=$(find build/conan -type f -name grpc_cpp_plugin -executable 2>/dev/null | head -n1 || true)
if [ -z "$PLUGIN" ]; then
    PLUGIN=$(find /root/.conan2 "$HOME/.conan2" -type f -name grpc_cpp_plugin -executable -print 2>/dev/null | head -n1 || true)
fi

PROTOC_BIN=$(find build/conan -type f -name protoc -executable 2>/dev/null | head -n1 || true)
if [ -z "$PROTOC_BIN" ]; then
    PROTOC_BIN=$(find /root/.conan2 "$HOME/.conan2" -type f -name protoc -executable -print 2>/dev/null | head -n1 || true)
fi

if [ -z "$PROTOC_BIN" ]; then
    mkdir -p build/tools
    ARCHIVE="/tmp/protoc_${PROTOC_VER}_linux_x86_64.zip"
    URL="https://github.com/protocolbuffers/protobuf/releases/download/v${PROTOC_VER}/protoc-${PROTOC_VER}-linux-x86_64.zip"
    curl -fSL --retry 3 -o "$ARCHIVE" "$URL"
    if [ ! -s "$ARCHIVE" ]; then
        echo "Download failed or archive empty: $ARCHIVE"
        exit 9
    fi
    unzip -o "$ARCHIVE" -d build/tools
    PROTOC_BIN="build/tools/bin/protoc"
    if [ ! -x "$PROTOC_BIN" ]; then
        echo "protoc not found after unzip"
        exit 9
    fi
    chmod +x "$PROTOC_BIN"
fi

export PATH="$(dirname "$PROTOC_BIN"):$PATH"

chmod +x raft_node/protogen.sh
./raft_node/protogen.sh "$PLUGIN"

chmod +x DHT/protogen.sh
./DHT/protogen.sh "$PLUGIN"

echo "Done."