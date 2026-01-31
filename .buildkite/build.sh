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
    tar \
    gzip \
    xz-utils \
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
    DEPS_DIR="/workdir/deps/windows-$ARCH"
    mkdir -p "$DEPS_DIR"
    export PATH="$DEPS_DIR/bin:$PATH"

    echo "--- :package: Building json-c for Windows ($ARCH)"
    if [ ! -f "$DEPS_DIR/lib/libjson-c.a" ]; then
        rm -rf json-c
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
        rm -rf curl-8.5.0*
        wget https://curl.se/download/curl-8.5.0.tar.gz
        tar xzf curl-8.5.0.tar.gz
        cd curl-8.5.0
        # Enable Schannel (native Windows SSL) and disable other features to simplify
        ./configure --host=$HOST --prefix="$DEPS_DIR" --disable-shared --enable-static --with-schannel --disable-ldap --disable-ldaps
        make
        make install
        cd ..
        rm -rf curl-8.5.0*
    fi

    echo "--- :rocket: Cross-compiling for Windows $ARCH"
    # Override CFLAGS to point to our manually built deps
    export CFLAGS="-I$DEPS_DIR/include"

    # Define libraries. Note: -lcurl needs -lbcrypt -lcrypt32 for Schannel?
    # Usually curl-config --static-libs would tell us, but we can't run it easily (it's a script).
    # We'll guess common Windows libs.
    export LIBS_windows="-L$DEPS_DIR/lib -lcurl -ljson-c -lws2_32 -lm -lbcrypt -ladvapi32 -lcrypt32 -lkernel32 -luser32"

    # We pass LIBS_windows to make to override the default.
    # We also explicitly pass the compiler to ensure make uses the right one.
    MAKE_CC="CC_windows-$ARCH=$HOST-gcc"
    if [ "$ARCH" == "i686" ]; then
         MAKE_CC="CC_windows-i386=$HOST-gcc"
    fi

    make cross-windows-$ARCH CFLAGS="$CFLAGS" LIBS_windows="$LIBS_windows" $MAKE_CC

elif [[ "$TARGET" == "macos-"* ]]; then
    ARCH="x86_64"
    if [ "$TARGET" == "macos-arm64" ]; then
        ARCH="arm64"
    fi

    echo "--- :mac: Installing Zig for MacOS cross-compilation"
    ZIG_VER="0.13.0"
    if [ ! -d "zig-linux-x86_64-$ZIG_VER" ]; then
        wget -q "https://ziglang.org/download/$ZIG_VER/zig-linux-x86_64-$ZIG_VER.tar.xz"
        tar -xf "zig-linux-x86_64-$ZIG_VER.tar.xz"
    fi
    export PATH="$PWD/zig-linux-x86_64-$ZIG_VER:$PATH"

    ZIG_TARGET="x86_64-macos"
    if [ "$ARCH" == "arm64" ]; then
        ZIG_TARGET="aarch64-macos"
    fi

    DEPS_DIR="/workdir/deps/macos-$ARCH"
    mkdir -p "$DEPS_DIR"

    # Create a wrapper script for CC to make it behave like a standard compiler
    echo '#!/bin/bash' > macos-cc
    echo "exec zig cc -target $ZIG_TARGET \"\$@\"" >> macos-cc
    chmod +x macos-cc
    export CC="$PWD/macos-cc"
    export CXX="$PWD/macos-cc" # Zig cc handles c++ too usually, but let's stick to C

    # Create wrappers for AR and RANLIB
    echo '#!/bin/bash' > macos-ar
    echo "exec zig ar \"\$@\"" >> macos-ar
    chmod +x macos-ar
    export AR="$PWD/macos-ar"

    echo '#!/bin/bash' > macos-ranlib
    echo "exec zig ranlib \"\$@\"" >> macos-ranlib
    chmod +x macos-ranlib
    export RANLIB="$PWD/macos-ranlib"

    echo "--- :package: Building json-c for MacOS ($ARCH)"
    if [ ! -f "$DEPS_DIR/lib/libjson-c.a" ]; then
        rm -rf json-c
        git clone https://github.com/json-c/json-c.git
        cd json-c
        mkdir build
        cd build
        cmake .. \
            -DCMAKE_SYSTEM_NAME=Darwin \
            -DCMAKE_C_COMPILER="$CC" \
            -DCMAKE_AR="$AR" \
            -DCMAKE_RANLIB="$RANLIB" \
            -DCMAKE_INSTALL_PREFIX="$DEPS_DIR" \
            -DBUILD_SHARED_LIBS=OFF \
            -DDISABLE_WERROR=ON
        make install
        cd ../..
        rm -rf json-c
    fi

    echo "--- :package: Building curl for MacOS ($ARCH)"
    if [ ! -f "$DEPS_DIR/lib/libcurl.a" ]; then
        rm -rf curl-8.5.0*
        wget https://curl.se/download/curl-8.5.0.tar.gz
        tar xzf curl-8.5.0.tar.gz
        cd curl-8.5.0
        # Use SecureTransport (deprecated but simple) or verify if Zig supports it.
        # Zig cross-compilation to MacOS links against SDK frameworks.
        # Curl's --with-secure-transport works with macOS SDK.
        ./configure --host=$ZIG_TARGET --prefix="$DEPS_DIR" --disable-shared --enable-static --with-secure-transport --disable-ldap --disable-ldaps
        make
        make install
        cd ..
        rm -rf curl-8.5.0*
    fi

    echo "--- :rocket: Cross-compiling for MacOS $ARCH"
    export CFLAGS="-I$DEPS_DIR/include"
    # Need to link CoreFoundation and Security frameworks for curl/SSL
    export LIBS_darwin="-L$DEPS_DIR/lib -lcurl -ljson-c -lm -lpthread -framework CoreFoundation -framework Security"

    # Makefile expects CC_darwin-x86_64 or CC_darwin-arm64
    MAKE_CC="CC_darwin-$ARCH=$CC"

    make cross-darwin-$ARCH CFLAGS="$CFLAGS" LIBS_darwin="$LIBS_darwin" $MAKE_CC

else
    echo "Unknown target: $TARGET"
    exit 1
fi

echo "--- :white_check_mark: Build Complete for $TARGET"
ls -la out/dist/
