# CDrive - A Cross-Platform Google Drive CLI Written in C

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-1.0.0-green.svg)](CHANGELOG.md)
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

### Option 1: Download Pre-built Binaries
1. Go to the [Releases](https://github.com/batuhantrkgl/CDrive/releases) page
2. Download the appropriate binary for your platform
3. Extract and move to your PATH:
   ```bash
   # Linux/macOS
   sudo mv cdrive /usr/local/bin/
   sudo chmod +x /usr/local/bin/cdrive
   ```

### Option 2: Build from Source
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

### Basic Build
```bash
git clone https://github.com/batuhantrkgl/CDrive.git
cd CDrive
make
```

### Debug Build
```bash
make debug
```

### Install System-wide
```bash
sudo make install
```

### Clean Build Files
```bash
make clean
```

## Cross-Platform Building

CDrive supports cross-compilation for multiple platforms:

### Available Targets
- `linux-x86_64` - Linux 64-bit (default, always available)
- `linux-i386` - Linux 32-bit (requires multilib support)
- `linux-arm64` - Linux ARM 64-bit (requires cross-compiler)
- `linux-armv7` - Linux ARM 32-bit (requires cross-compiler)
- `windows-x86_64` - Windows 64-bit (requires MinGW)
- `windows-i386` - Windows 32-bit (requires MinGW)
- `darwin-x86_64` - macOS Intel (requires OSXCross)
- `darwin-arm64` - macOS Apple Silicon (requires OSXCross)

**Note**: Targets without required toolchains will be automatically skipped with a warning message.

### Build Commands

```bash
# Build for specific platform
make TARGET=linux-x86_64

# Build all platforms
make all-platforms

# Create distribution packages
make dist

# Build and package everything
make release
```

### Cross-Compilation Requirements

For cross-compilation, you'll need the appropriate toolchains:

#### 32-bit Support (from 64-bit Linux)
```bash
# Ubuntu/Debian
sudo apt install gcc-multilib libc6-dev-i386
sudo apt install libcurl4-openssl-dev:i386 libjson-c-dev:i386

# Fedora/RHEL/CentOS
sudo dnf install glibc-devel.i686 libgcc.i686
sudo dnf install libcurl-devel.i686 json-c-devel.i686
```

#### Windows (from Linux)
```bash
# Ubuntu/Debian
sudo apt install gcc-mingw-w64 mingw-w64-tools

# Fedora
sudo dnf install mingw64-gcc mingw32-gcc
```

#### ARM (from x86_64)
```bash
# Ubuntu/Debian
sudo apt install gcc-aarch64-linux-gnu gcc-arm-linux-gnueabihf

# Fedora
sudo dnf install gcc-aarch64-linux-gnu gcc-arm-linux-gnu
```

#### macOS Cross-Compilation
```bash
# Requires OSXCross (complex setup)
# See: https://github.com/tpoechtrager/osxcross
# Note: macOS targets are rarely needed and require Apple SDK
```

**Note**: Some targets may not be available on all systems. If a cross-compilation target fails, ensure you have the required toolchain and development libraries installed for that architecture.

### Troubleshooting Cross-Compilation

**32-bit compilation fails with "unrecognized command-line option '-m32'":**
```bash
# Ubuntu/Debian
sudo apt install gcc-multilib libc6-dev-i386
sudo dpkg --add-architecture i386
sudo apt update
sudo apt install libcurl4-openssl-dev:i386 libjson-c-dev:i386

# Fedora/RHEL/CentOS
sudo dnf install glibc-devel.i686 libgcc.i686
sudo dnf install libcurl-devel.i686 json-c-devel.i686
```

**Skip failed targets and continue:**
```bash
# Build only specific working targets
make TARGET=linux-x86_64
make TARGET=windows-x86_64

# The build system automatically skips unavailable targets
# Example output: "Warning: Cross-compiler not found, skipping target"
```

**Common cross-compiler package names:**
```bash
# For ARM support
sudo apt install gcc-aarch64-linux-gnu gcc-arm-linux-gnueabihf  # Ubuntu/Debian
sudo dnf install gcc-aarch64-linux-gnu gcc-arm-linux-gnu        # Fedora

# For Windows support  
sudo apt install gcc-mingw-w64                                  # Ubuntu/Debian
sudo dnf install mingw64-gcc mingw32-gcc                        # Fedora
```

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