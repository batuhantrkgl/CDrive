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
VERSION ?= $(shell grep 'CDRIVE_VERSION' cdrive.h | cut -d'"' -f2)
SOURCES = main.c auth.c upload.c spinner.c version.c download.c

# Build directories
OUT_DIR = out
DIST_DIR = $(OUT_DIR)/dist
OBJ_DIR = $(OUT_DIR)/obj
DEP_DIR = $(OBJ_DIR)/deps

# Host system detection
ifeq ($(OS),Windows_NT)
	HOST_OS_NAME := windows
	HOST_ARCH := $(PROCESSOR_ARCHITECTURE)
	ifeq ($(HOST_ARCH),AMD64)
		HOST_ARCH := x86_64
	endif
	ifeq ($(HOST_ARCH),x86)
		HOST_ARCH := i386
	endif
else
	HOST_OS_NAME := $(shell uname -s | tr '[:upper:]' '[:lower:]')
	HOST_ARCH := $(shell uname -m)
endif
HOST_TARGET = $(HOST_OS_NAME)-$(HOST_ARCH)

# Compiler and flags
CC ?= gcc
BASE_CFLAGS = -Wall -Wextra -std=c99 -O2
CFLAGS ?= $(BASE_CFLAGS)
DEBUG_CFLAGS = -Wall -Wextra -std=c99 -g -DDEBUG

# Use pkg-config for host build flags
PKG_LIBS = libcurl json-c
HOST_CFLAGS = $(shell pkg-config --cflags $(PKG_LIBS))
HOST_LIBS = $(shell pkg-config --libs $(PKG_LIBS)) -lm -lpthread

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
LDFLAGS_windows-x86_64 = -static
LDFLAGS_windows-i386 = -static
EXT_windows-x86_64 = .exe
EXT_windows-i386 = .exe

# Define platform-specific libraries for cross-compilation
LIBS_windows = -lcurl -ljson-c -lws2_32 -lm
LIBS_linux = -lcurl -ljson-c -lm -lpthread
LIBS_darwin = -lcurl -ljson-c -lm -lpthread


# Object files
OBJECTS = $(SOURCES:%.c=$(OBJ_DIR)/%.o)

# Default target - build for host system
all: $(DIST_DIR)/$(PROJECT_NAME)

# Build for host system
$(DIST_DIR)/$(PROJECT_NAME): $(OBJECTS)
	@mkdir -p $(DIST_DIR)
	@printf "$(CYAN)Linking $(BOLD)$(PROJECT_NAME)$(RESET)$(CYAN) for $(YELLOW)$(HOST_TARGET)$(RESET)$(CYAN)...$(RESET)\n"
	$(CC) $(OBJECTS) -o $@ $(HOST_LIBS)
	@printf "$(GREEN)Build complete: $(BOLD)$@$(RESET)\n"

# Compile source files
$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(OBJ_DIR) $(DEP_DIR)
	@printf "$(BLUE)Compiling $(BOLD)$<$(RESET)$(BLUE)...$(RESET)\n"
	$(CC) $(CFLAGS) $(HOST_CFLAGS) -MMD -MP -MF $(DEP_DIR)/$*.d -c $< -o $@

# Include generated dependency files
-include $(SOURCES:%.c=$(DEP_DIR)/%.d)

# Debug build
debug: CFLAGS = $(DEBUG_CFLAGS)
debug: all

cross-all: $(addprefix cross-, $(TARGETS))

# Generic rule for cross-compiling. It's inefficient as it recompiles all sources,
# but centralizes the logic from the CI workflow.
define cross_compile_rule
cross-$(1): $(DIST_DIR)/$(PROJECT_NAME)-$(1)$(EXT_$(1))

$(DIST_DIR)/$(PROJECT_NAME)-$(1)$(EXT_$(1)): $(SOURCES) cdrive.h | $(DIST_DIR)
	@printf "$(YELLOW)Cross-compiling for $(BOLD)$(1)$(RESET)$(YELLOW)...$(RESET)\n"
	@if command -v $(CC_$(1)) >/dev/null 2>&1; then \
		$(CC_$(1)) $(CFLAGS) $(LDFLAGS_$(1)) $(SOURCES) -o $$@ $(LIBS_$(word 1,$(subst -, ,$(1)))) && \
		printf "$(GREEN)Cross-compilation complete: $(BOLD)$$@$(RESET)\n" || \
		(printf "$(RED)Cross-compilation failed for $(1)$(RESET)\n"; exit 1); \
	else \
		printf "$(RED)Warning: Cross-compiler $(CC_$(1)) not found, skipping $(1)$(RESET)\n"; \
	fi
endef

# Generate cross-compilation rules
$(foreach target,$(TARGETS),$(eval $(call cross_compile_rule,$(target))))

# Release build - creates archives for distribution
release: clean $(addprefix cross-, $(TARGETS))
	@printf "$(CYAN)Creating release archives...$(RESET)\n"
	@cd $(DIST_DIR) && \
	for file in $(PROJECT_NAME)-*; do \
		if [ -f "$$file" ]; then \
			platform=$$(echo $$file | sed 's/$(PROJECT_NAME)-//' | sed 's/\.exe$$//'); \
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
	@printf "$(BOLD)$(BLUE)Host: $(WHITE)$(HOST_TARGET)$(RESET)\n"
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

.PHONY: all debug cross-all release clean install uninstall deps check-cross test info help $(addprefix cross-, $(TARGETS))