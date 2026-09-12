<div align="center">

# CDrive

**A lightweight, high-performance command-line interface for Google Drive written in pure C.**

[![Fedora COPR](https://img.shields.io/badge/Fedora%20COPR-batuhantrkgl%2Fcdrive-blue?logo=fedora)](https://copr.fedorainfracloud.org/coprs/batuhantrkgl/cdrive/)
[![Copr build status](https://copr.fedorainfracloud.org/coprs/batuhantrkgl/cdrive/package/cdrive/status_image/last_build.png)](https://copr.fedorainfracloud.org/coprs/batuhantrkgl/cdrive/)
[![Version](https://img.shields.io/badge/version-1.0.3-brightgreen)](https://github.com/batuhantrkgl/CDrive/releases)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows%20%7C%20macOS-lightgrey)](#cross-compilation)
[![Dependencies](https://img.shields.io/badge/dependencies-libcurl%20%7C%20json--c-orange)](#prerequisites)

<p align="center">
  <a href="#features">Features</a> •
  <a href="#installation">Installation</a> •
  <a href="#quick-start">Quick Start</a> •
  <a href="#command-reference">Commands</a> •
  <a href="#configuration">Configuration</a> •
  <a href="#building-from-source">Building</a> •
  <a href="#support">Support</a> •
  <a href="#license">License</a>
</p>

</div>

---

## Features

- **Fast & Resource-Efficient**: Written in standard C99 with near-instant startup time and minimal RAM usage.
- **OAuth2 Authentication**:
  - Local callback server with automatic browser launch.
  - Headless terminal mode (`--no-browser`) for remote servers and SSH sessions.
  - Automatic token refreshing and credential fingerprinting.
  - Native support for standard Google Cloud Console `credentials.json` files.
- **Interactive File Browser**: Navigate Google Drive folders directly from your terminal using arrow keys, breadcrumb navigation, and direct downloads.
- **Resumable Downloads**: Automatic HTTP byte-range resumption with transfer speeds, progress bars, and ETA calculations.
- **Multi-File Uploads**: Supports multiple files, wildcards (`*.pdf`), target destination folders, and outputs direct download and web-view links.
- **File Search**: Name search with safe query escaping for spaces and special characters.
- **Access Sharing**: Share files with custom permissions (`reader`, `writer`, `commenter`, etc.) directly from the CLI.
- **Scripting & Automation Ready**: Global `--json` flag formats all command outputs (`list`, `search`, `upload`, `auth`) into structured JSON for scripting and CI/CD pipelines.
- **Self-Updating**: Automated GitHub release checking and compilation options (`cdrive update`).

---

## Installation

### Fedora / RHEL / CentOS (Fedora COPR)

CDrive is packaged and hosted on [Fedora COPR](https://copr.fedorainfracloud.org/coprs/batuhantrkgl/cdrive/):

```bash
# Enable the COPR repository
sudo dnf copr enable batuhantrkgl/cdrive

# Install CDrive
sudo dnf install cdrive
```

---

### Pre-Built Binaries (Linux, macOS, Windows)

Download standalone binaries for your platform from the [GitHub Releases](https://github.com/batuhantrkgl/CDrive/releases) page:

#### Linux & macOS
```bash
# Extract the archive
tar -xzf cdrive-<platform>.tar.gz

# Move to system PATH
sudo mv cdrive /usr/local/bin/
sudo chmod +x /usr/local/bin/cdrive
```

#### Windows
Download `cdrive-windows-x86_64.zip`, extract `cdrive.exe`, and add its folder to your system `PATH`.

---

### Build from Source

```bash
git clone https://github.com/batuhantrkgl/CDrive.git
cd CDrive
make
sudo make install
```

---

## Quick Start

### 1. Authenticate

```bash
# Standard browser-assisted authentication
cdrive auth login

# Headless / SSH remote session authentication
cdrive auth login --no-browser
```

### 2. Basic Operations

```bash
# List files in your Drive root
cdrive list

# Create a new directory
cdrive mkdir "Backups"

# Upload files to a specific folder
cdrive upload report.pdf 1BxiMVs...pU

# Upload multiple files using wildcards
cdrive upload documents/*.pdf

# Search files by name
cdrive search "Finance"

# Download a file by its ID (fetches remote filename automatically)
cdrive pull 1BxiMVs...pU

# Browse and select files interactively
cdrive pull
```

---

## Command Reference

### Authentication (`auth`)

```bash
cdrive auth login              # Authenticate via browser callback (port 8080)
cdrive auth login --no-browser # Headless authorization code copy-paste
cdrive auth status             # Display active token fingerprint and status
cdrive auth logout             # Revoke and erase saved OAuth tokens
```

### File Management

| Command | Syntax | Description |
| :--- | :--- | :--- |
| **`list`** | `cdrive list [folder_id]` | List files and folders in tabular format (or root) |
| **`mkdir`** | `cdrive mkdir <folder_name> [parent_id]` | Create a new directory |
| **`upload`** | `cdrive upload <files...> [folder_id]` | Upload single or multiple files with progress |
| **`pull`** | `cdrive pull [file_id] [output_name]` | Download a file, or launch interactive browser |
| **`search`** | `cdrive search <query>` | Search files matching `<query>` |
| **`share`** | `cdrive share <id> --email <email> [--role <role>]` | Share file (`reader`, `writer`, `commenter`) |

### Scripting & JSON Mode

Pass `--json` anywhere to receive machine-readable JSON:

```bash
# JSON file listing
cdrive list root --json

# JSON search results
cdrive search "invoice" --json

# JSON upload response (status, file ID, download link, web link)
cdrive upload photo.jpg --json
```

### Updates & Maintenance

```bash
cdrive version          # Show current version and check for new releases
cdrive update --check   # Force an update check bypassing the 4-hour cache
cdrive update --auto    # Download and replace binary with the latest release
cdrive update --compile # Automatically fetch latest source and compile locally
```

---

## Configuration

### Google Cloud OAuth Setup

1. Open the [Google Cloud Console](https://console.cloud.google.com/).
2. Create or select a project.
3. Enable the **Google Drive API** under **APIs & Services > Library**.
4. Navigate to **APIs & Services > Credentials** and click **Create Credentials > OAuth client ID**.
   - **Application Type**: `Desktop app`
   - **Name**: `CDrive`
5. Download your credentials JSON.
6. Either enter the Client ID and Secret when prompted by `cdrive auth login`, or save the JSON file directly:
   ```bash
   mkdir -p ~/.cdrive
   cp credentials.json ~/.cdrive/client_id.json
   ```

### Configuration Storage

CDrive stores configuration files under `~/.cdrive/`:

| Path | Purpose |
| :--- | :--- |
| `~/.cdrive/client_id.json` | OAuth2 Client ID and Client Secret |
| `~/.cdrive/token.json` | Access & Refresh tokens (managed automatically) |
| `~/.cdrive/update_cache.json` | Release check cache (expires every 4 hours) |

---

## Building & Cross-Compilation

### Prerequisites

| Distro / OS | Command |
| :--- | :--- |
| **Fedora / RHEL** | `sudo dnf install gcc make libcurl-devel json-c-devel` |
| **Ubuntu / Debian** | `sudo apt install build-essential libcurl4-openssl-dev libjson-c-dev` |
| **Arch Linux** | `sudo pacman -S base-devel curl json-c` |
| **macOS** | `brew install curl json-c` |

### Makefile Targets

```bash
make                 # Build native binary into out/dist/cdrive
make clean           # Remove build artifacts
make install         # Install binary (supports PREFIX=/usr DESTDIR=...)
make srpm            # Generate Source RPM for Fedora COPR / packaging
make release         # Package release tarballs for distribution
make test            # Smoke test built executable
```

### Supported Cross-Compilation Targets

CDrive includes cross-compilation rules in the `Makefile`:

- **Linux**: `linux-x86_64`, `linux-i386`, `linux-arm64`, `linux-armv7`
- **Windows**: `windows-x86_64`, `windows-i386` (MinGW-w64)
- **macOS**: `darwin-x86_64`, `darwin-arm64` (OSXCross)

To cross-compile for a specific platform:
```bash
make windows-x86_64
make linux-arm64
```

---

## Support

If CDrive makes your workflow easier and you would like to support ongoing development:

[![Buy Me A Coffee](https://img.shields.io/badge/Buy%20Me%20A%20Coffee-batuhantrkgl-yellow?style=for-the-badge&logo=buy-me-a-coffee&logoColor=black)](https://buymeacoffee.com/batuhantrkgl)

You can sponsor or buy a coffee at [buymeacoffee.com/batuhantrkgl](https://buymeacoffee.com/batuhantrkgl).

---

## License

This project is licensed under the **MIT License**. See the [LICENSE](LICENSE) file for details.

---

<div align="center">
  <sub>Maintained by <a href="https://github.com/batuhantrkgl">Batuhan Türkoğlu</a></sub>
</div>
