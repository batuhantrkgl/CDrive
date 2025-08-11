# Makefile for cdrive CLI tool

# Colors for better output
RESET = \033[0m
BOLD = \033[1m
RED = \033[31m
GREEN = \033[32m
YELLOW = \033[33m
BLUE = \033[34m
MAGENTA = \033[35m
CYAN = \033[36m
WHITE = \033[37m

# Project configuration
PROJECT_NAME = cdrive
VERSION = 1.0.2
SOURCES = main.c auth.c upload.c spinner.c version.c

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
LIBS ?= -lcurl -ljson-c -lm -lpthread

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
	@printf "$(CYAN)Linking $(BOLD)$(PROJECT_NAME)$(RESET)$(CYAN) for $(YELLOW)$(HOST_OS)-$(HOST_ARCH)$(RESET)$(CYAN)...$(RESET)\n"
	$(CC) $(OBJECTS) -o $@ $(LIBS)
	@printf "$(GREEN)Build complete: $(BOLD)$@$(RESET)\n"

# Compile source files
$(OBJ_DIR)/%.o: %.c cdrive.h | $(OBJ_DIR)
	@printf "$(BLUE)Compiling $(BOLD)$<$(RESET)$(BLUE)...$(RESET)\n"
	$(CC) $(CFLAGS) -c $< -o $@

# Debug build
debug: CFLAGS = $(DEBUG_CFLAGS)
debug: $(DIST_DIR)/$(PROJECT_NAME)-debug

$(DIST_DIR)/$(PROJECT_NAME)-debug: $(OBJECTS) | $(DIST_DIR)
	@printf "$(MAGENTA)Linking $(BOLD)$(PROJECT_NAME)$(RESET)$(MAGENTA) (debug) for $(YELLOW)$(HOST_OS)-$(HOST_ARCH)$(RESET)$(MAGENTA)...$(RESET)\n"
	$(CC) $(OBJECTS) -o $@ $(LIBS)
	@printf "$(GREEN)Debug build complete: $(BOLD)$@$(RESET)\n"

# Cross-compilation targets
cross-all: $(TARGETS)

# Template for cross-compilation
define cross_compile_template
$(1): $(DIST_DIR)/$(PROJECT_NAME)-$(1)$(EXT_$(word 1,$(subst -, ,$(1))))

$(DIST_DIR)/$(PROJECT_NAME)-$(1)$(EXT_$(word 1,$(subst -, ,$(1)))): $(SOURCES) cdrive.h | $(DIST_DIR)
	@printf "$(YELLOW)Cross-compiling for $(BOLD)$(1)$(RESET)$(YELLOW)...$(RESET)\n"
	@if command -v $(CC_$(1)) >/dev/null 2>&1; then \
		$(CC_$(1)) $(CFLAGS) $(LDFLAGS_$(word 1,$(subst -, ,$(1)))) $(SOURCES) -o $$@ $(LIBS) && \
		printf "$(GREEN)Cross-compilation complete: $(BOLD)$$@$(RESET)\n"; \
	else \
		printf "$(RED)Warning: Cross-compiler $(CC_$(1)) not found, skipping $(1)$(RESET)\n"; \
	fi
endef

# Generate cross-compilation rules
$(foreach target,$(TARGETS),$(eval $(call cross_compile_template,$(target))))

# Release build - creates archives for distribution
release: clean cross-all
	@printf "$(CYAN)Creating release archives...$(RESET)\n"
	@cd $(DIST_DIR) && \
	for file in $(PROJECT_NAME)-*; do \
		if [ -f "$$file" ]; then \
			platform=$$(echo $$file | sed 's/$(PROJECT_NAME)-//'); \
			mkdir -p "$$platform"; \
			cp "$$file" "$$platform/$(PROJECT_NAME)$$(echo $$file | sed 's/.*\(\.[^.]*\)$$/\1/' | grep -E '\.(exe)$$' || echo '')"; \
			cp ../README.md "$$platform/" 2>/dev/null || echo "# $(PROJECT_NAME)" > "$$platform/README.md"; \
			tar -czf "$(PROJECT_NAME)-$(VERSION)-$$platform.tar.gz" "$$platform"; \
			rm -rf "$$platform"; \
			printf "$(GREEN)Created: $(BOLD)$(PROJECT_NAME)-$(VERSION)-$$platform.tar.gz$(RESET)\n"; \
		fi; \
	done
	@printf "$(GREEN)Release archives created in $(BOLD)$(DIST_DIR)/$(RESET)\n"

# Clean build artifacts
clean:
	@printf "$(YELLOW)Cleaning build artifacts...$(RESET)\n"
	@rm -rf $(OUT_DIR)
	@printf "$(GREEN)Clean complete!$(RESET)\n"

# Install to system (requires sudo)
install: $(DIST_DIR)/$(PROJECT_NAME)
	@printf "$(CYAN)Installing $(BOLD)$(PROJECT_NAME)$(RESET)$(CYAN) to /usr/local/bin...$(RESET)\n"
	@sudo cp $(DIST_DIR)/$(PROJECT_NAME) /usr/local/bin/
	@sudo chmod +x /usr/local/bin/$(PROJECT_NAME)
	@printf "$(GREEN)Installation complete! You can now run '$(BOLD)$(PROJECT_NAME)$(RESET)$(GREEN)' from anywhere.$(RESET)\n"

# Uninstall from system
uninstall:
	@printf "$(YELLOW)Uninstalling $(BOLD)$(PROJECT_NAME)$(RESET)$(YELLOW)...$(RESET)\n"
	@sudo rm -f /usr/local/bin/$(PROJECT_NAME)
	@printf "$(GREEN)Uninstallation complete!$(RESET)\n"

# Check dependencies
deps:
	@printf "$(BLUE)Checking dependencies...$(RESET)\n"
	@which gcc > /dev/null || (printf "$(RED)ERROR: gcc not found. Install with: sudo dnf install gcc$(RESET)\n" && exit 1)
	@pkg-config --exists libcurl || (printf "$(RED)ERROR: libcurl not found. Install with: sudo dnf install libcurl-devel$(RESET)\n" && exit 1)
	@pkg-config --exists json-c || (printf "$(RED)ERROR: json-c not found. Install with: sudo dnf install json-c-devel$(RESET)\n" && exit 1)
	@printf "$(GREEN)All dependencies are installed!$(RESET)\n"

# Check cross-compilation tools
check-cross:
	@printf "$(BLUE)Checking cross-compilation tools...$(RESET)\n"
	@for target in $(TARGETS); do \
		compiler=$$(echo "$(CC_$$target)" | cut -d' ' -f1); \
		if command -v $$compiler >/dev/null 2>&1; then \
			printf "$(GREEN)✓ $$target: $$compiler$(RESET)\n"; \
		else \
			printf "$(RED)✗ $$target: $$compiler (not available)$(RESET)\n"; \
		fi; \
	done

# Test the build
test: $(DIST_DIR)/$(PROJECT_NAME)
	@printf "$(BLUE)Testing $(BOLD)$(PROJECT_NAME)$(RESET)$(BLUE)...$(RESET)\n"
	@$(DIST_DIR)/$(PROJECT_NAME) help >/dev/null && printf "$(GREEN)Test passed!$(RESET)\n" || printf "$(RED)Test failed!$(RESET)\n"

# Show build info
info:
	@printf "$(BOLD)$(CYAN)Project: $(WHITE)$(PROJECT_NAME) v$(VERSION)$(RESET)\n"
	@printf "$(BOLD)$(BLUE)Host: $(WHITE)$(HOST_OS)-$(HOST_ARCH)$(RESET)\n"
	@printf "$(BOLD)$(GREEN)Sources: $(WHITE)$(SOURCES)$(RESET)\n"
	@printf "$(BOLD)$(YELLOW)Output: $(WHITE)$(DIST_DIR)/$(PROJECT_NAME)$(RESET)\n"
	@printf "$(BOLD)$(MAGENTA)Targets: $(WHITE)$(TARGETS)$(RESET)\n"

# Show help
help:
	@printf "$(BOLD)$(CYAN)$(PROJECT_NAME) Build System$(RESET)\n"
	@printf "\n"
	@printf "$(BOLD)$(BLUE)Targets:$(RESET)\n"
	@printf "  $(GREEN)all$(RESET)         - Build for host system (default)\n"
	@printf "  $(GREEN)debug$(RESET)       - Build debug version\n"
	@printf "  $(GREEN)cross-all$(RESET)   - Cross-compile for all platforms\n"
	@printf "  $(GREEN)release$(RESET)     - Create release archives for all platforms\n"
	@printf "  $(GREEN)clean$(RESET)       - Clean all build artifacts\n"
	@printf "  $(GREEN)install$(RESET)     - Install to /usr/local/bin (requires sudo)\n"
	@printf "  $(GREEN)uninstall$(RESET)   - Remove from /usr/local/bin (requires sudo)\n"
	@printf "  $(GREEN)deps$(RESET)        - Check build dependencies\n"
	@printf "  $(GREEN)check-cross$(RESET) - Check cross-compilation tools\n"
	@printf "  $(GREEN)test$(RESET)        - Test the build\n"
	@printf "  $(GREEN)info$(RESET)        - Show build information\n"
	@printf "  $(GREEN)help$(RESET)        - Show this help\n"
	@printf "\n"
	@printf "$(BOLD)$(YELLOW)Cross-compilation targets:$(RESET)\n"
	@for target in $(TARGETS); do printf "  $(CYAN)$$target$(RESET)\n"; done
	@printf "\n"
	@printf "$(BOLD)$(MAGENTA)Output directories:$(RESET)\n"
	@printf "  $(CYAN)$(OBJ_DIR)/$(RESET)     - Object files\n"
	@printf "  $(CYAN)$(DIST_DIR)/$(RESET)    - Executables and archives\n"

.PHONY: all debug cross-all release clean install uninstall deps check-cross test info help $(TARGETS)
