# CDrive

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-1.0.2-green)](https://github.com/batuhantrkgl/CDrive/releases)
[![Release Date](https://img.shields.io/badge/release-2025--08--11-orange)](https://github.com/batuhantrkgl/CDrive/releases)
[![Build](https://img.shields.io/badge/build-passing-brightgreen)](https://github.com/batuhantrkgl/CDrive/actions)
[![Platform](https://img.shields.io/badge/platform-linux%20%7C%20windows%20%7C%20macos-lightgrey)](#cross-compilation)
[![Language](https://img.shields.io/badge/language-C-555555)](https://en.wikipedia.org/wiki/C_(programming_language))
[![curl](https://img.shields.io/badge/libcurl-8.19.0-blue)](https://curl.se/libcurl/)
[![json-c](https://img.shields.io/badge/json--c-0.18-blue)](https://github.com/json-c/json-c)

A professional, lightweight command-line interface for Google Drive written in C. CDrive provides fast, efficient file operations with Google Drive using minimal system resources.

---

## Features

- **OAuth2 Authentication** -- Secure login with automatic token refresh and headless mode
- **File Upload** -- Single and glob-pattern uploads with real-time progress and ETA
- **File Download** -- Resumable downloads with partial-file recovery, progress bars, and ETA
- **Search** -- Name-based file search across your Drive
- **File Sharing** -- Share files with configurable roles (reader, writer, commenter)
- **Glob Expansion** -- Native wildcard support (`*`, `?`, `[...]`) on all platforms
- **JSON Output** -- `--json` flag for scripting and programmatic use
- **Cross-Platform** -- Linux (x86_64, i386, ARM64, ARMv7), Windows (x86_64, i386), macOS (Intel, Apple Silicon)
- **Lightweight** -- Single binary, no runtime dependencies beyond libcurl and json-c
- **Colored Output** -- Professional terminal interface with ANSI colors and spinner animations

---

## Table of Contents

- [Installation](#installation)
- [Quick Start](#quick-start)
- [Commands](#commands)
- [Building from Source](#building-from-source)
- [Cross-Compilation](#cross-compilation)
- [Configuration](#configuration)
- [SSH Usage](#ssh-usage)
- [Development](#development)
- [Contributing](#contributing)
- [License](#license)

---

## Installation

### Pre-built Binaries

Download the latest release for your platform from the [Releases page](https://github.com/batuhantrkgl/CDrive/releases):

```bash
# Linux / macOS
tar -xzf cdrive-<platform>.tar.gz
sudo mv cdrive /usr/local/bin/
sudo chmod +x /usr/local/bin/cdrive

# Windows
# Extract the .exe and add it to your PATH
```

### Build from Source

```bash
git clone https://github.com/batuhantrkgl/CDrive.git
cd CDrive
make
sudo make install
```

---

## Quick Start

1. **Obtain OAuth2 credentials** from the [Google Cloud Console](https://console.cloud.google.com/) (see [Configuration](#configuration)).
2. **Authenticate:**

    ```bash
    cdrive auth login
    ```

3. **Upload a file:**

    ```bash
    cdrive upload document.pdf
    ```

4. **List your files:**

    ```bash
    cdrive list
    ```

5. **Download a file by ID:**

    ```bash
    cdrive pull <file-id>
    ```

---

## Commands

### Authentication

| Command | Description |
|---------|-------------|
| `cdrive auth login` | OAuth2 login with browser or headless (`--no-browser`) |
| `cdrive auth status` | Show token fingerprint and authentication state |

### File Operations

| Command | Description |
|---------|-------------|
| `cdrive upload <source> [folder-id]` | Upload file(s) -- supports glob patterns |
| `cdrive list [folder-id]` | List files and folders |
| `cdrive mkdir <name> [parent-id]` | Create a new folder |
| `cdrive pull [file-id]` | Download by ID, or browse and select interactively |
| `cdrive search <query>` | Search files by name (supports `--json`) |
| `cdrive share <file-id> --email <email> [--role <role>]` | Share a file (roles: reader, writer, commenter) |

### Utility

| Command | Description |
|---------|-------------|
| `cdrive version` | Show version and check for updates |
| `cdrive update --check` | Check for updates |
| `cdrive update --auto` | Download and install latest version |
| `cdrive help` | Show usage |

### Global Flags

| Flag | Description |
|------|-------------|
| `--json` | Output machine-readable JSON (currently supported by `search`) |

### Examples

```bash
# Upload all PDFs in current directory
cdrive upload "*.pdf"

# Upload to a specific folder
cdrive upload photo.jpg 1BxiMVs0XRA5nFMdKvBdBZjgmUUqptlbs74mMYEon3pU

# Search with JSON output
cdrive search "meeting notes" --json

# Share a file
cdrive share 1BxiMVs0XRA5nFMdKvBdBZjgmUUqptlbs74mMYEon3pU --email user@example.com --role writer

# Resume an interrupted download
cdrive pull 1BxiMVs0XRA5nFMdKvBdBZjgmUUqptlbs74mMYEon3pU
```

---

## Building from Source

### Prerequisites

- **libcurl** (>= 7.64.0)
- **json-c** (>= 0.13)
- GCC or Clang (C99)

### Linux

```bash
# Debian / Ubuntu
sudo apt install libcurl4-openssl-dev libjson-c-dev build-essential

# Fedora / RHEL
sudo dnf install libcurl-devel json-c-devel gcc make

# Arch Linux
sudo pacman -S curl json-c base-devel
```

### Windows (MSYS2 / MinGW64)

```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-curl mingw-w64-x86_64-json-c mingw-w64-x86_64-make
gcc -I/mingw64/include -L/mingw64/lib main.c auth.c upload.c spinner.c version.c download.c -o cdrive.exe -lcurl -ljson-c -lws2_32 -lm
```

### macOS

```bash
brew install curl json-c
make
```

### Build Targets

```bash
make               # Build for host system
make debug         # Build with debug symbols and -DDEBUG
make clean         # Remove build artifacts
make install       # Install to /usr/local/bin
make test          # Run the binary and verify it starts
make deps          # Check build dependencies
make info          # Show build configuration
```

---

## Cross-Compilation

CDrive supports cross-compilation for 8 target platforms.

### Targets

| Target | Arch | Toolchain |
|--------|------|-----------|
| `linux-x86_64` | AMD64/Intel 64-bit | `gcc` (native) |
| `linux-i386` | IA-32 32-bit | `gcc -m32` |
| `linux-arm64` | AArch64 | `aarch64-linux-gnu-gcc` |
| `linux-armv7` | ARM hard-float | `arm-linux-gnueabihf-gcc` |
| `windows-x86_64` | Win64 | `x86_64-w64-mingw32-gcc` |
| `windows-i386` | Win32 | `i686-w64-mingw32-gcc` |
| `darwin-x86_64` | macOS Intel | `o64-clang` (OSXCross) |
| `darwin-arm64` | macOS Apple Silicon | `o64-clang` (OSXCross) |

### Commands

```bash
make cross-all                        # Build all available targets
make release                          # Create .tar.gz archives for distribution
make windows-x86_64                   # Build for Windows 64-bit only
make check-cross                      # List available cross-compilers
```

Missing toolchains are automatically skipped with a warning.

---

## Configuration

### Google Cloud Setup

1. Go to the [Google Cloud Console](https://console.cloud.google.com/).
2. Create a project or select an existing one.
3. Enable the **Google Drive API** (APIs & Services > Library).
4. Create **OAuth 2.0 credentials** (APIs & Services > Credentials).
   - Application type: **Desktop application**
   - Redirect URI: `http://localhost:8080`
5. Download the JSON credentials file.

### First Login

Run `cdrive auth login`. On first invocation you will be prompted to enter your `client_id` and `client_secret` (from the downloaded JSON). These are saved to `~/.cdrive/client_id.json`.

### File Locations

| File | Purpose |
|------|---------|
| `~/.cdrive/client_id.json` | OAuth2 client credentials (user-provided) |
| `~/.cdrive/token.json` | Access and refresh tokens (auto-managed) |
| `~/.cdrive/update_cache.json` | Update check cache (auto-managed) |

---

## SSH Usage

When running on a remote server, forward port 8080 for browser-based authentication:

```bash
ssh -L 8080:localhost:8080 user@your-server
```

Alternatively, use headless mode:

```bash
cdrive auth login --no-browser
```

---

## Project Structure

```
CDrive/
  main.c        -- Entry point, command dispatch, --json flag
  auth.c        -- OAuth2 flow, token management, cdrive_api_get helper
  upload.c      -- Upload with progress, search, share, glob expansion
  download.c    -- Resumable download, interactive file browser
  spinner.c     -- Threaded animated spinner
  version.c     -- Version display, update checking, self-update
  cdrive.h      -- Types, constants, macro definitions
  compat.h      -- Portable clock, sleep, socket, getch wrappers
  download.h    -- Download function declarations
  Makefile      -- Build system with cross-compilation support
```

---

## Contributing

1. Fork the repository.
2. Create a feature branch.
3. Commit your changes.
4. Open a pull request.

For major changes, please open an issue first to discuss what you would like to change.

---

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.

---

## Acknowledgments

- [libcurl](https://curl.se/libcurl/) -- HTTP client library
- [json-c](https://github.com/json-c/json-c) -- JSON parsing library
- [Google Drive API](https://developers.google.com/drive/api) -- Google Drive integration

---

[Report Bug](https://github.com/batuhantrkgl/CDrive/issues) &middot; [Request Feature](https://github.com/batuhantrkgl/CDrive/issues)
