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

else
    echo "Unknown target: $TARGET"
    exit 1
fi

echo "--- :white_check_mark: Build Complete for $TARGET"
ls -la out/dist/
