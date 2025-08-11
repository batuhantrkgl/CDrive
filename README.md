# CDrive - A Cross-Platform Google Drive CLI Written in C

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-1.0.2-green.svg)](CHANGELOG.md)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows%20%7C%20macOS-lightgrey.svg)](#installation)

A professional, lightweight command-line interface for Google Drive written in C. CDrive provides fast, efficient file operations with Google Drive using minimal system resources.

CDrive brings Google Drive to your terminal, following the same design principles as GitHub CLI.

## Features

- **OAuth2 Authentication** - Secure Google Drive authentication
- **File Upload** - Upload files to Google Drive with progress indication
- **Folder Management** - Create and organize folders
- **File Listing** - Browse Google Drive contents
- **Colored Output** - Beautiful terminal interface with colors
- **Cross-Platform** - Works on Linux, Windows, and macOS
- **Fast & Lightweight** - Written in C for optimal performance
- **Token Management** - Automatic token refresh and secure storage

## Table of Contents

- [Installation](#installation)
- [Prerequisites](#prerequisites)
- [Quick Start](#quick-start)
- [Usage](#usage)
- [Commands](#commands)
- [Configuration](#configuration)
- [Building from Source](#building-from-source)
- [Cross-Platform Building](#cross-platform-building)
- [Contributing](#contributing)
- [License](#license)

## Prerequisites

Before using CDrive, ensure you have the following dependencies installed:

### Required Libraries
- **libcurl** - For HTTP requests
- **json-c** - For JSON parsing

### Installation on Different Platforms

#### Ubuntu/Debian
```bash
sudo apt update
sudo apt install libcurl4-openssl-dev libjson-c-dev build-essential
```

#### CentOS/RHEL/Fedora
```bash
# For RHEL/CentOS with yum
sudo yum install libcurl-devel json-c-devel gcc make

# For Fedora with dnf
sudo dnf install libcurl-devel json-c-devel gcc make
```

#### Arch Linux
```bash
sudo pacman -S curl json-c base-devel
```

#### macOS
```bash
# Using Homebrew
brew install curl json-c

# Using MacPorts
sudo port install curl json-c
```

#### Windows (WSL/MinGW)
```bash
# For WSL (Windows Subsystem for Linux)
sudo apt install libcurl4-openssl-dev libjson-c-dev build-essential

# For MinGW-w64
pacman -S mingw-w64-x86_64-curl mingw-w64-x86_64-json-c
```

## Installation

### Option 1: Download Latest Build Artifacts
1. Go to the [Actions](https://github.com/batuhantrkgl/CDrive/actions) page
2. Click on the latest successful workflow run
3. Download the appropriate binary for your platform from the artifacts section
4. Extract and move to your PATH:
   ```bash
   # Linux/macOS
   tar -xzf cdrive-<platform>.tar.gz
   sudo mv cdrive /usr/local/bin/
   sudo chmod +x /usr/local/bin/cdrive
   ```

### Option 2: Download from Releases (Automated)
1. Go to the [Releases](https://github.com/batuhantrkgl/CDrive/releases) page
2. Download the appropriate binary for your platform from the latest release
3. Extract and move to your PATH:
   ```bash
   # Linux/macOS
   tar -xzf cdrive-<platform>.tar.gz
   sudo mv cdrive /usr/local/bin/
   sudo chmod +x /usr/local/bin/cdrive
   ```

### Option 3: Build from Source
```bash
git clone https://github.com/batuhantrkgl/CDrive.git
cd CDrive
make
sudo make install
```

## Quick Start

1. **Set up Google Drive API credentials** (see [Configuration](#configuration))
2. **Authenticate with Google Drive:**
   ```bash
   cdrive auth login
   ```
3. **Upload a file:**
   ```bash
   cdrive upload ./myfile.txt
   ```
4. **List files:**
   ```bash
   cdrive list
   ```

## Usage

```bash
cdrive <command> [flags]
```

### Basic Examples

```bash
# Authenticate with Google Drive
cdrive auth login

# Check authentication status  
cdrive auth status

# Upload a file to root folder
cdrive upload document.pdf

# Upload a file to specific folder
cdrive upload photo.jpg 1BxiMVs0XRA5nFMdKvBdBZjgmUUqptlbs74mMYEon3pU

# List files in root folder
cdrive list

# List files in specific folder  
cdrive list 1BxiMVs0XRA5nFMdKvBdBZjgmUUqptlbs74mMYEon3pU

# Create a new folder
cdrive mkdir "My Documents"

# Create a folder in specific parent folder
cdrive mkdir "Subfolder" 1BxiMVs0XRA5nFMdKvBdBZjgmUUqptlbs74mMYEon3pU

# Show version
cdrive --version

# Show help
cdrive --help
```

## Commands

### Authentication Commands

| Command | Description |
|---------|-------------|
| `cdrive auth login` | Authenticate with Google Drive using OAuth2 |
| `cdrive auth status` | Show current authentication status |

### File Operations

| Command | Description | Example |
|---------|-------------|---------|
| `cdrive upload <file> [folder_id]` | Upload a file to Google Drive | `cdrive upload ./doc.pdf` |
| `cdrive list [folder_id]` | List files in Google Drive folder | `cdrive list` |
| `cdrive mkdir <name> [parent_id]` | Create a new folder | `cdrive mkdir "Photos"` |

### Utility Commands

| Command | Description |
|---------|-------------|
| `cdrive --version` | Show version information |
| `cdrive --help` | Show help information |

## Configuration

### Setting up Google Drive API

1. **Create a Google Cloud Project:**
   - Go to [Google Cloud Console](https://console.cloud.google.com/)
   - Create a new project or select existing one

2. **Enable Google Drive API:**
   - Navigate to "APIs & Services" > "Library"
   - Search for "Google Drive API"
   - Click "Enable"

3. **Create OAuth2 Credentials:**
   - Go to "APIs & Services" > "Credentials"
   - Click "Create Credentials" > "OAuth client ID"
   - Choose "Desktop application"
   - Add the redirect URI: `http://localhost:8080/callback`
   - Download the JSON file

4. **Configure CDrive:**
   - When you run `cdrive auth login` for the first time, the program will prompt you to enter:
     - Client ID (from the downloaded JSON file)
     - Client Secret (from the downloaded JSON file)
   - CDrive will automatically create and manage the configuration files

### Configuration Files

CDrive stores its configuration in `~/.cdrive/`:
- `client_id.json` - Your Google OAuth2 credentials (auto-generated)
- `token.json` - Access and refresh tokens (auto-generated)

## Building from Source

### Quick Start
```bash
git clone https://github.com/batuhantrkgl/CDrive.git
cd CDrive
make deps    # Check dependencies
make         # Build for current platform
```

### Build Options

#### Standard Build
```bash
make                 # Build for host system
make debug          # Build with debug symbols
make clean          # Clean build artifacts
```

#### Advanced Build with Custom Flags
```bash
# Custom compiler and flags
CC=clang CFLAGS="-O3 -march=native" make

# Custom library paths (useful for cross-compilation)
LIBS="$(pkg-config --libs libcurl json-c)" make

# 32-bit build on 64-bit system
CC="gcc -m32" CFLAGS="-m32 -O2" LIBS="-lcurl -ljson-c" make
```

#### System Integration
```bash
sudo make install   # Install to /usr/local/bin
sudo make uninstall # Remove from system
make test           # Test the built binary
```

#### Utility Commands
```bash
make info           # Show build configuration
make deps           # Check dependencies
make check-cross    # Check cross-compilation tools
make help           # Show all available targets
```

## Cross-Platform Building

CDrive supports cross-compilation for multiple platforms with automatic toolchain detection and pkg-config integration.

### Available Targets
- `linux-x86_64` - Linux 64-bit (default, always available)
- `linux-i386` - Linux 32-bit (requires multilib support)
- `linux-arm64` - Linux ARM 64-bit (requires cross-compiler)
- `linux-armhf` - Linux ARM with hardware floating point (requires cross-compiler)
- `windows-x86_64` - Windows 64-bit (requires MinGW)
- `windows-i386` - Windows 32-bit (requires MinGW)
- `darwin-x86_64` - macOS Intel (requires OSXCross)
- `darwin-arm64` - macOS Apple Silicon (requires OSXCross)

**Note**: Targets without required toolchains will be automatically skipped with a warning message.

### Cross-Compilation Commands

#### Basic Cross-Compilation
```bash
# Build for specific platforms (using cross-compilation targets)
make linux-i386        # 32-bit Linux
make linux-arm64       # ARM64 Linux
make windows-x86_64     # 64-bit Windows

# Build all available platforms
make cross-all

# Create release packages for all platforms
make release
```

#### Advanced Build with Custom Settings
The build system supports conditional variable assignment, allowing you to override compiler, flags, and libraries:

```bash
# Custom compiler and optimization
CC=clang CFLAGS="-O3 -march=native" make

# Cross-compilation with pkg-config (automated in CI/CD)
CC="gcc -m32" \
CFLAGS="-m32 $(pkg-config --cflags libcurl json-c)" \
LIBS="$(pkg-config --libs libcurl json-c)" make

# ARM cross-compilation
CC=aarch64-linux-gnu-gcc \
CFLAGS="$(pkg-config --cflags libcurl json-c)" \
LIBS="$(pkg-config --libs libcurl json-c)" make
```

### Cross-Compilation Setup

For cross-compilation, you'll need the appropriate toolchains:

#### 32-bit Support (i386 from x86_64 Linux)
```bash
# Ubuntu/Debian - comprehensive setup
sudo apt-get update
sudo apt-get install -y build-essential gcc-multilib libc6-dev-i386
sudo dpkg --add-architecture i386
sudo apt-get update
sudo apt-get install -y libcurl4-openssl-dev:i386 libjson-c-dev:i386

# Fedora/RHEL/CentOS
sudo dnf install glibc-devel.i686 libgcc.i686
sudo dnf install libcurl-devel.i686 json-c-devel.i686
```

#### ARM Support (ARM64/ARMHF)
```bash
# Ubuntu/Debian
sudo apt-get install gcc-aarch64-linux-gnu gcc-arm-linux-gnueabihf
sudo dpkg --add-architecture arm64 armhf
sudo apt-get update
sudo apt-get install libcurl4-openssl-dev:arm64 libjson-c-dev:arm64
sudo apt-get install libcurl4-openssl-dev:armhf libjson-c-dev:armhf

# Fedora
sudo dnf install gcc-aarch64-linux-gnu gcc-arm-linux-gnu
```

#### Windows Cross-Compilation (from Linux)
```bash
# Ubuntu/Debian
sudo apt-get install gcc-mingw-w64 mingw-w64-tools

# Fedora
sudo dnf install mingw64-gcc mingw32-gcc
```

#### macOS Cross-Compilation (Advanced)
```bash
# Requires OSXCross (complex setup)
# See: https://github.com/tpoechtrager/osxcross
# Note: Requires Apple SDK and is mainly for CI/CD environments
```

### Build System Features

#### Automatic Toolchain Detection
The build system automatically detects available cross-compilers and skips unavailable targets:

```bash
make check-cross  # Check available cross-compilation tools
```

#### pkg-config Integration
The build system uses pkg-config to automatically find correct library paths for cross-compilation, avoiding hardcoded paths and improving compatibility across different Linux distributions.

#### Conditional Variable Assignment
All major build variables use conditional assignment (`?=`), allowing easy customization:

```bash
# Override any build variable
CC=clang make                    # Use Clang instead of GCC
CFLAGS="-O3 -flto" make         # Custom optimization flags
LIBS="$(pkg-config --libs libcurl json-c)" make  # Custom library paths
```

### Troubleshooting Cross-Compilation

#### 32-bit Compilation Issues
**Error: "unrecognized command-line option '-m32'"**
```bash
# Ubuntu/Debian
sudo apt-get install gcc-multilib libc6-dev-i386
**Error: "cannot find -lcurl" or "cannot find -ljson-c"**
```bash
# For i386 builds, ensure i386 libraries are installed
sudo dpkg --add-architecture i386
sudo apt-get update
sudo apt-get install libcurl4-openssl-dev:i386 libjson-c-dev:i386

# Use pkg-config for correct library paths
PKG_CONFIG_PATH="/usr/lib/i386-linux-gnu/pkgconfig" \
CC="gcc -m32" CFLAGS="-m32" \
LIBS="$(pkg-config --libs libcurl json-c)" make
```

#### Library Path Issues
**Error: "Package 'libcurl' was not found" during cross-compilation**
```bash
# Set correct pkg-config path for target architecture
export PKG_CONFIG_PATH="/usr/lib/arm-linux-gnueabihf/pkgconfig"
export PKG_CONFIG_LIBDIR="/usr/lib/arm-linux-gnueabihf/pkgconfig"

# Verify libraries are found
pkg-config --exists libcurl json-c && echo "Libraries found" || echo "Libraries missing"
```

#### Build Target Issues
**Some cross-compilation targets fail:**
```bash
# Check available cross-compilers
make check-cross

# Build only available targets
make linux-x86_64    # Always works (native)
make linux-i386      # If multilib installed

# The build system automatically skips unavailable targets
# Example output: "Warning: Cross-compiler not found, skipping target"
```

#### Quick Cross-Compilation Reference
```bash
# Check what's available
make info            # Show build configuration
make check-cross     # List available cross-compilers
make deps           # Verify dependencies

# Common working combinations
make                # Native build (always works)
make cross-all      # Build all available targets
make release        # Create distribution packages

# Manual cross-compilation with custom settings
CC="gcc -m32" CFLAGS="-m32" LIBS="-lcurl -ljson-c" make  # i386
CC="aarch64-linux-gnu-gcc" make linux-arm64             # ARM64
```

The build system is designed to be robust and will automatically skip targets that can't be built, so you can safely run `make cross-all` even if not all cross-compilers are installed.

### Automated Builds (GitHub Actions)

CDrive includes a comprehensive CI/CD pipeline that automatically builds for all supported platforms:

#### Supported CI/CD Platforms
- **Linux**: x86_64, i386, ARM64, ARMHF
- **Windows**: x86_64, i386  
- **macOS**: x86_64 (Intel), ARM64 (Apple Silicon)

#### Features
- **Automatic builds** on every push to the repository
- **Manual trigger support** for on-demand builds
- **pkg-config integration** for correct cross-compilation library paths
- **Platform-specific optimizations** and dependency management
- **Artifact generation** for easy distribution

#### Triggering Manual Builds
You can manually trigger builds from the GitHub Actions tab with optional debug mode:

```yaml
# The workflow supports manual dispatch with debug option
# Available in GitHub repository → Actions → Build Multi-Platform → Run workflow
```

The CI/CD system uses the same Makefile and build logic as local development, ensuring consistency between local and automated builds.

#### Automated Releases
CDrive features an automated release system that:
- **Auto-generates versions** based on date and commit count (e.g., `2025.08.11.42-a1b2c3d`)
- **Updates README.md** automatically with new version badges
- **Creates GitHub releases** with all platform binaries and checksums
- **Provides comprehensive release notes** with recent changes and installation instructions

To trigger a release:
1. Push to `main` or `master` branch (automatic release)
2. Or use **Actions → Build and Release → Run workflow** with "Create release" enabled

All releases include binaries for Linux (x86_64, i386, ARM64, ARMHF), Windows (x86_64, i386), and macOS (Intel, Apple Silicon).

## Output Features

CDrive provides a beautiful terminal interface with:
- **Colored output** for better readability
- **Success/error indicators** with appropriate colors
- **Progress information** during uploads
- **Detailed file information** in listings

## Development

### Project Structure
```
cdrive/
├── main.c          # Main application entry point
├── auth.c          # OAuth2 authentication logic
├── upload.c        # File upload functionality
├── cdrive.h        # Header file with declarations
├── Makefile        # Build configuration
└── README.md       # This file
```

### Adding New Features

1. Fork the repository
2. Create a feature branch
3. Add your functionality
4. Update tests if applicable
5. Submit a pull request

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request. For major changes, please open an issue first to discuss what you would like to change.

### Development Setup
```bash
git clone https://github.com/batuhantrkgl/CDrive.git
cd CDrive
make debug
```

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- [libcurl](https://curl.se/libcurl/) - HTTP client library
- [json-c](https://github.com/json-c/json-c) - JSON parsing library
- [Google Drive API](https://developers.google.com/drive/api) - Google Drive integration

## Support

- **Bug Reports**: [GitHub Issues](https://github.com/batuhantrkgl/CDrive/issues)
- **Feature Requests**: [GitHub Issues](https://github.com/batuhantrkgl/CDrive/issues)
- **Email**: info@batuhantrkgl.tech

## Roadmap

- [ ] File download functionality
- [ ] Folder synchronization
- [ ] Batch operations
- [ ] Configuration file support
- [ ] Plugin system
- [ ] Terminal User Interface (TUI)

---

**Made with ❤️ in C**