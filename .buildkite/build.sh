#!/bin/bash
set -euo pipefail

echo "--- :package: Installing dependencies"
export DEBIAN_FRONTEND=noninteractive

apt-get update
apt-get install -y \
    build-essential \
    pkg-config \
    curl \
    git \
    libcurl4-openssl-dev \
    libjson-c-dev

# Detect OS codename
CODENAME=$(grep VERSION_CODENAME /etc/os-release | cut -d= -f2)
echo "Detected Ubuntu codename: $CODENAME"

echo "--- :linux: Installing cross-compilation tools (i386)"
dpkg --add-architecture i386
apt-get update
apt-get install -y \
    gcc-multilib \
    libc6-dev-i386 \
    libjson-c-dev:i386
apt-get install -y -o Dpkg::Options::="--force-overwrite" libcurl4-openssl-dev:i386

echo "--- :linux: Installing cross-compilation tools (arm64)"
dpkg --add-architecture arm64
apt-get install -y gcc-aarch64-linux-gnu

# Add ports for arm64
sed -i 's/deb http/deb [arch=amd64] http/g' /etc/apt/sources.list
echo "deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports $CODENAME main universe" >> /etc/apt/sources.list
echo "deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports $CODENAME-updates main universe" >> /etc/apt/sources.list

apt-get update || true
apt-get install -y libjson-c-dev:arm64
apt-get install -y -o Dpkg::Options::="--force-overwrite" libcurl4-openssl-dev:arm64

echo "--- :hammer: Building Native"
make

echo "--- :test_tube: Testing Native"
make test

echo "--- :rocket: Cross-compiling"

# Set PKG_CONFIG_PATH for i386
export PKG_CONFIG_PATH="/usr/lib/i386-linux-gnu/pkgconfig"
make cross-linux-i386

# Set PKG_CONFIG_PATH for arm64
export PKG_CONFIG_PATH="/usr/lib/aarch64-linux-gnu/pkgconfig"
make cross-linux-arm64

echo "--- :white_check_mark: Build Complete"
ls -la out/dist/