# Makefile for toralizer project

# Directories
SRC_DIR = ../src
INCLUDE_DIR = ../include
OBJ_DIR = ../obj
BUILD_DIR = .

# Compiler and flags
CC = gcc
CFLAGS = -fPIC -shared -ldl -D_GNU_SOURCE -I$(INCLUDE_DIR)
TARGET = $(BUILD_DIR)/toralize.so

# Source and object files
SRC = $(SRC_DIR)/toralize.c
OBJ = $(OBJ_DIR)/toralize.o

# Default target
all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(CFLAGS)

$(OBJ): $(SRC)
	@mkdir -p $(OBJ_DIR)
	$(CC) -c $< -o $@ $(CFLAGS)

# Clean up
clean:
	rm -rf $(OBJ_DIR)/*.o $(TARGET)

.PHONY: all clean
