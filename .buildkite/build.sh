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
    lsb-release \
    wget \
    unzip \
    cmake \
    autoconf \
    libtool \
    automake

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

    dpkg --add-architecture arm64
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
    apt-get install -y gcc-aarch64-linux-gnu libcurl4-openssl-dev:arm64 libjson-c-dev:arm64

    echo "--- :rocket: Cross-compiling for arm64"
    export PKG_CONFIG_LIBDIR="/usr/lib/aarch64-linux-gnu/pkgconfig"
    make cross-linux-arm64

elif [[ "$TARGET" == "windows-"* ]]; then
    ARCH="x86_64"
    HOST="x86_64-w64-mingw32"
    if [ "$TARGET" == "windows-i386" ]; then
        ARCH="i686"
        HOST="i686-w64-mingw32"
    fi

    echo "--- :windows: Installing Windows ($ARCH) toolchain"
    apt-get install -y mingw-w64

    # Create deps directory
    DEPS_DIR="/workdir/deps/$ARCH"
    mkdir -p "$DEPS_DIR"
    export PATH="$DEPS_DIR/bin:$PATH"

    echo "--- :package: Building json-c for Windows ($ARCH)"
    if [ ! -f "$DEPS_DIR/lib/libjson-c.a" ]; then
        git clone https://github.com/json-c/json-c.git
        cd json-c
        mkdir build
        cd build
        cmake .. \
            -DCMAKE_SYSTEM_NAME=Windows \
            -DCMAKE_C_COMPILER=$HOST-gcc \
            -DCMAKE_INSTALL_PREFIX="$DEPS_DIR" \
            -DBUILD_SHARED_LIBS=OFF \
            -DDISABLE_WERROR=ON
        make install
        cd ../..
        rm -rf json-c
    fi

    echo "--- :package: Building curl for Windows ($ARCH)"
    if [ ! -f "$DEPS_DIR/lib/libcurl.a" ]; then
        wget https://curl.se/download/curl-8.5.0.tar.gz
        tar xzf curl-8.5.0.tar.gz
        cd curl-8.5.0
        ./configure --host=$HOST --prefix="$DEPS_DIR" --disable-shared --enable-static --without-ssl --disable-ldap --disable-ldaps
        make
        make install
        cd ..
        rm -rf curl-8.5.0*
    fi

    echo "--- :rocket: Cross-compiling for Windows $ARCH"
    # Override CFLAGS to point to our manually built deps
    export CFLAGS="-I$DEPS_DIR/include"

    # The Makefile uses LIBS_windows for the windows targets. We need to override that.
    # We also need to add -lbcrypt and others that static curl might need.
    export LIBS_windows="-L$DEPS_DIR/lib -lcurl -ljson-c -lws2_32 -lm -lbcrypt -ladvapi32 -lcrypt32"

    # We pass LIBS_windows to make to override the default.
    make cross-windows-$ARCH CFLAGS="$CFLAGS" LIBS_windows="$LIBS_windows"

else
    echo "Unknown target: $TARGET"
    exit 1
fi

echo "--- :white_check_mark: Build Complete for $TARGET"
ls -la out/dist/
