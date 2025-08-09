# Makefile for cdrive CLI tool

# Project configuration
PROJECT_NAME = cdrive
VERSION = 1.0.0
SOURCES = main.c auth.c upload.c

# Build directories
OUT_DIR = out
DIST_DIR = $(OUT_DIR)/dist
OBJ_DIR = $(OUT_DIR)/obj

# Host system detection
HOST_OS := $(shell uname -s)
HOST_ARCH := $(shell uname -m)

# Compiler and flags
CC ?= gcc
CFLAGS ?= -Wall -Wextra -std=c99 -O2
DEBUG_CFLAGS = -Wall -Wextra -std=c99 -g -DDEBUG
LIBS ?= -lcurl -ljson-c

# Target platforms for cross-compilation
TARGETS = \
	linux-x86_64 \
	linux-i386 \
	linux-arm64 \
	linux-armv7 \
	windows-x86_64 \
	windows-i386 \
	darwin-x86_64 \
	darwin-arm64

# Cross-compiler configurations
CC_linux-x86_64 = gcc
CC_linux-i386 = gcc -m32
CC_linux-arm64 = aarch64-linux-gnu-gcc
CC_linux-armv7 = arm-linux-gnueabihf-gcc
CC_windows-x86_64 = x86_64-w64-mingw32-gcc
CC_windows-i386 = i686-w64-mingw32-gcc
CC_darwin-x86_64 = o64-clang
CC_darwin-arm64 = o64-clang

# Platform-specific settings
LDFLAGS_windows = -static
EXT_windows = .exe
EXT_linux = 
EXT_darwin = 

# Object files
OBJECTS = $(SOURCES:%.c=$(OBJ_DIR)/%.o)

# Default target - build for host system
all: $(DIST_DIR)/$(PROJECT_NAME)

# Create directories
$(OUT_DIR):
	@mkdir -p $(OUT_DIR)

$(OBJ_DIR): | $(OUT_DIR)
	@mkdir -p $(OBJ_DIR)

$(DIST_DIR): | $(OUT_DIR)
	@mkdir -p $(DIST_DIR)

# Build for host system
$(DIST_DIR)/$(PROJECT_NAME): $(OBJECTS) | $(DIST_DIR)
	@echo "Linking $(PROJECT_NAME) for $(HOST_OS)-$(HOST_ARCH)..."
	$(CC) $(OBJECTS) -o $@ $(LIBS)
	@echo "Build complete: $@"

# Compile source files
$(OBJ_DIR)/%.o: %.c cdrive.h | $(OBJ_DIR)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Debug build
debug: CFLAGS = $(DEBUG_CFLAGS)
debug: $(DIST_DIR)/$(PROJECT_NAME)-debug

$(DIST_DIR)/$(PROJECT_NAME)-debug: $(OBJECTS) | $(DIST_DIR)
	@echo "Linking $(PROJECT_NAME) (debug) for $(HOST_OS)-$(HOST_ARCH)..."
	$(CC) $(OBJECTS) -o $@ $(LIBS)
	@echo "Debug build complete: $@"

# Cross-compilation targets
cross-all: $(TARGETS)

# Template for cross-compilation
define cross_compile_template
$(1): $(DIST_DIR)/$(PROJECT_NAME)-$(1)$(EXT_$(word 1,$(subst -, ,$(1))))

$(DIST_DIR)/$(PROJECT_NAME)-$(1)$(EXT_$(word 1,$(subst -, ,$(1)))): $(SOURCES) cdrive.h | $(DIST_DIR)
	@echo "Cross-compiling for $(1)..."
	@if command -v $(CC_$(1)) >/dev/null 2>&1; then \
		$(CC_$(1)) $(CFLAGS) $(LDFLAGS_$(word 1,$(subst -, ,$(1)))) $(SOURCES) -o $$@ $(LIBS) && \
		echo "Cross-compilation complete: $$@"; \
	else \
		echo "Warning: Cross-compiler $(CC_$(1)) not found, skipping $(1)"; \
	fi
endef

# Generate cross-compilation rules
$(foreach target,$(TARGETS),$(eval $(call cross_compile_template,$(target))))

# Release build - creates archives for distribution
release: clean cross-all
	@echo "Creating release archives..."
	@cd $(DIST_DIR) && \
	for file in $(PROJECT_NAME)-*; do \
		if [ -f "$$file" ]; then \
			platform=$$(echo $$file | sed 's/$(PROJECT_NAME)-//'); \
			mkdir -p "$$platform"; \
			cp "$$file" "$$platform/$(PROJECT_NAME)$$(echo $$file | sed 's/.*\(\.[^.]*\)$$/\1/' | grep -E '\.(exe)$$' || echo '')"; \
			cp ../README.md "$$platform/" 2>/dev/null || echo "# $(PROJECT_NAME)" > "$$platform/README.md"; \
			tar -czf "$(PROJECT_NAME)-$(VERSION)-$$platform.tar.gz" "$$platform"; \
			rm -rf "$$platform"; \
			echo "Created: $(PROJECT_NAME)-$(VERSION)-$$platform.tar.gz"; \
		fi; \
	done
	@echo "Release archives created in $(DIST_DIR)/"

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(OUT_DIR)
	@echo "Clean complete!"

# Install to system (requires sudo)
install: $(DIST_DIR)/$(PROJECT_NAME)
	@echo "Installing $(PROJECT_NAME) to /usr/local/bin..."
	@sudo cp $(DIST_DIR)/$(PROJECT_NAME) /usr/local/bin/
	@sudo chmod +x /usr/local/bin/$(PROJECT_NAME)
	@echo "Installation complete! You can now run '$(PROJECT_NAME)' from anywhere."

# Uninstall from system
uninstall:
	@echo "Uninstalling $(PROJECT_NAME)..."
	@sudo rm -f /usr/local/bin/$(PROJECT_NAME)
	@echo "Uninstallation complete!"

# Check dependencies
deps:
	@echo "Checking dependencies..."
	@which gcc > /dev/null || (echo "ERROR: gcc not found. Install with: sudo dnf install gcc" && exit 1)
	@pkg-config --exists libcurl || (echo "ERROR: libcurl not found. Install with: sudo dnf install libcurl-devel" && exit 1)
	@pkg-config --exists json-c || (echo "ERROR: json-c not found. Install with: sudo dnf install json-c-devel" && exit 1)
	@echo "All dependencies are installed!"

# Check cross-compilation tools
check-cross:
	@echo "Checking cross-compilation tools..."
	@for target in $(TARGETS); do \
		compiler=$$(echo "$(CC_$$target)" | cut -d' ' -f1); \
		if command -v $$compiler >/dev/null 2>&1; then \
			echo "✓ $$target: $$compiler"; \
		else \
			echo "✗ $$target: $$compiler (not available)"; \
		fi; \
	done

# Test the build
test: $(DIST_DIR)/$(PROJECT_NAME)
	@echo "Testing $(PROJECT_NAME)..."
	@$(DIST_DIR)/$(PROJECT_NAME) help >/dev/null && echo "Test passed!" || echo "Test failed!"

# Show build info
info:
	@echo "Project: $(PROJECT_NAME) v$(VERSION)"
	@echo "Host: $(HOST_OS)-$(HOST_ARCH)"
	@echo "Sources: $(SOURCES)"
	@echo "Output: $(DIST_DIR)/$(PROJECT_NAME)"
	@echo "Targets: $(TARGETS)"

# Show help
help:
	@echo "$(PROJECT_NAME) Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all         - Build for host system (default)"
	@echo "  debug       - Build debug version"
	@echo "  cross-all   - Cross-compile for all platforms"
	@echo "  release     - Create release archives for all platforms"
	@echo "  clean       - Clean all build artifacts"
	@echo "  install     - Install to /usr/local/bin (requires sudo)"
	@echo "  uninstall   - Remove from /usr/local/bin (requires sudo)"
	@echo "  deps        - Check build dependencies"
	@echo "  check-cross - Check cross-compilation tools"
	@echo "  test        - Test the build"
	@echo "  info        - Show build information"
	@echo "  help        - Show this help"
	@echo ""
	@echo "Cross-compilation targets:"
	@echo "  $(TARGETS)" | tr ' ' '\n' | sed 's/^/  /'
	@echo ""
	@echo "Output directories:"
	@echo "  $(OBJ_DIR)/     - Object files"
	@echo "  $(DIST_DIR)/    - Executables and archives"

.PHONY: all debug cross-all release clean install uninstall deps check-cross test info help $(TARGETS)
