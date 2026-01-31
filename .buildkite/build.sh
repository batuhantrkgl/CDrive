#!/bin/bash
set -euo pipefail

TARGET=${1:-native}

echo "--- :package: Installing common dependencies"
export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y \
    build-essential \
    pkg-config \
    curl \
    git \
    lsb-release

# Detect OS codename
CODENAME=$(lsb_release -sc)
echo "Detected Ubuntu codename: $CODENAME"

if [ "$TARGET" = "native" ]; then
    echo "--- :linux: Installing native dependencies"
    apt-get install -y \
        libcurl4-openssl-dev \
        libjson-c-dev

    echo "--- :hammer: Building Native"
    make

    echo "--- :test_tube: Testing Native"
    make test

elif [ "$TARGET" = "linux-i386" ]; then
    echo "--- :linux: Installing i386 cross-compilation tools"
    dpkg --add-architecture i386
    apt-get update
    apt-get install -y \
        gcc-multilib \
        libc6-dev-i386 \
        libcurl4-openssl-dev:i386 \
        libjson-c-dev:i386

    echo "--- :rocket: Cross-compiling for i386"
    export PKG_CONFIG_LIBDIR="/usr/lib/i386-linux-gnu/pkgconfig"
    make cross-linux-i386

elif [ "$TARGET" = "linux-arm64" ]; then
    echo "--- :linux: Installing arm64 cross-compilation tools"

    # Enable arm64 architecture
    dpkg --add-architecture arm64

    # Setup repositories for multi-arch (amd64 on archive, arm64 on ports)
    # Remove existing sources to avoid conflicts/404s
    rm -f /etc/apt/sources.list.d/*.sources

    echo "Writing new sources.list..."
    cat > /etc/apt/sources.list <<EOF
deb [arch=amd64] http://archive.ubuntu.com/ubuntu/ $CODENAME main restricted universe multiverse
deb [arch=amd64] http://archive.ubuntu.com/ubuntu/ $CODENAME-updates main restricted universe multiverse
deb [arch=amd64] http://security.ubuntu.com/ubuntu/ $CODENAME-security main restricted universe multiverse
deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports/ $CODENAME main restricted universe multiverse
deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports/ $CODENAME-updates main restricted universe multiverse
deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports/ $CODENAME-security main restricted universe multiverse
EOF

    apt-get update

    # Install cross-compiler and libraries
    apt-get install -y gcc-aarch64-linux-gnu
    apt-get install -y libcurl4-openssl-dev:arm64 libjson-c-dev:arm64

    echo "--- :rocket: Cross-compiling for arm64"
    export PKG_CONFIG_LIBDIR="/usr/lib/aarch64-linux-gnu/pkgconfig"
    make cross-linux-arm64

elif [ "$TARGET" = "windows-x86_64" ]; then
    echo "--- :windows: Installing Windows (x86_64) cross-compilation tools"
    apt-get install -y mingw-w64

    # Note: mingw-w64 usually includes headers/libs, but specific deps like libcurl might need manual handling or
    # finding a mingw build of them. However, for this task, we will attempt the basic cross-compile.
    # Often, CI environments use separate images or package managers for this.
    # For now, we install the compiler. If deps are missing, we might need 'make deps' logic or failure is expected.
    # But usually 'libcurl' for windows is not in standard ubuntu repos for mingw.
    # Checking if the Makefile handles this... it says:
    # LIBS_windows = -lcurl -ljson-c -lws2_32 -lm
    # It assumes libraries are available.
    # In standard Ubuntu, we might not have 'libcurl' for MinGW pre-packaged.
    # But let's try.

    echo "--- :rocket: Cross-compiling for Windows x86_64"
    make cross-windows-x86_64

elif [ "$TARGET" = "windows-i386" ]; then
    echo "--- :windows: Installing Windows (i386) cross-compilation tools"
    apt-get install -y mingw-w64

    echo "--- :rocket: Cross-compiling for Windows i386"
    make cross-windows-i386

else
    echo "Unknown target: $TARGET"
    exit 1
fi

echo "--- :white_check_mark: Build Complete for $TARGET"
ls -la out/dist/
