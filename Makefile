# Root-level Makefile for toralizer
# =====================================

# You can override installation directories with:
#   make PREFIX=/your/install/path
PREFIX        ?= /usr/local
BINDIR        := $(PREFIX)/bin
LIBDIR        := $(PREFIX)/lib/toralizer

# Directories for source, headers, objects, and build outputs
SRC_DIR       := src
INCLUDE_DIR   := include
OBJ_DIR       := obj
BUILD_DIR     := build

# Filenames
SO_NAME       := toralizer.so
SCRIPT_SRC    := toralize.sh       # source script in repo root
SCRIPT_NAME   := toralize.sh       # within build dir

# Compiler settings
CC            := gcc
CFLAGS        := -Wall -Wextra -fPIC -I$(INCLUDE_DIR)
LDFLAGS       := -shared

# Gather sources and corresponding object files
SRCS          := $(wildcard $(SRC_DIR)/*.c)
OBJS          := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

# Default targets
.PHONY: all clean install uninstall
all: $(BUILD_DIR) $(OBJ_DIR) \
	$(BUILD_DIR)/$(SO_NAME) \
	$(BUILD_DIR)/$(SCRIPT_NAME)

# Create dirs as needed
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile each source into position-independent object
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Link shared library into build dir
$(BUILD_DIR)/$(SO_NAME): $(OBJS) | $(BUILD_DIR)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

# Copy shell wrapper if present in root, else assume it's already in build/
ifeq ($(wildcard $(SCRIPT_SRC)),)
$(BUILD_DIR)/$(SCRIPT_NAME): | $(BUILD_DIR)
	@echo "Using existing $(BUILD_DIR)/$(SCRIPT_NAME)"
else
$(BUILD_DIR)/$(SCRIPT_NAME): $(SCRIPT_SRC) | $(BUILD_DIR)
	cp $< $@
	chmod +x $@
endif

# Remove only objects and the shared object; leave the script intact
clean:
	rm -rf $(OBJ_DIR)
	rm -f $(BUILD_DIR)/$(SO_NAME)

# Install to system locations (use sudo if needed)
install: all
	@echo "Installing to $(PREFIX)..."
	install -d $(DESTDIR)$(LIBDIR)
	install -m 755 $(BUILD_DIR)/$(SO_NAME) $(DESTDIR)$(LIBDIR)/
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(BUILD_DIR)/$(SCRIPT_NAME) $(DESTDIR)$(BINDIR)/toralize
	@echo "Installed library to $(LIBDIR) and wrapper to $(BINDIR)/toralize"

# Remove installed files
uninstall:
	@echo "Removing installed files from $(PREFIX)..."
	rm -f $(DESTDIR)$(BINDIR)/toralize
	rm -rf $(DESTDIR)$(LIBDIR)
	@echo "Uninstalled."

