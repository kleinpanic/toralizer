
# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -fPIC -shared -ldl -D_GNU_SOURCE -I../include

# Directories
SRC_DIR = ../src
INCLUDE_DIR = ../include
OBJ_DIR = ../obj
BUILD_DIR = .

# Output shared object
TARGET = $(BUILD_DIR)/toralizer.so

# Source files
SRCS = $(SRC_DIR)/toralize.c

# Build target
$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

# Clean up
clean:
	rm -rf $(TARGET)

# Rebuild
rebuild: clean $(TARGET)

# Phony targets
.PHONY: clean rebuild
