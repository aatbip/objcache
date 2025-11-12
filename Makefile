# Compiler to use
CC = gcc 

# Compiler flags
CFLAGS = -Iinclude -Wall -g -MD

# Source and build directories
SRC_DIR = src
BUILD_DIR = build
INCLUDE_DIR = include

# Automatically find all .c files in src/
SOURCES = $(wildcard $(SRC_DIR)/*.c)

# Convert source files to object files (e.g., src/objcache.c -> build/objcache.o)
OBJECTS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SOURCES))

# Dependency files for tracking headers
DEPS = $(OBJECTS:.o=.d)

# Name of the final executable
EXEC = $(BUILD_DIR)/objcache

# Default target
all: $(EXEC)

# Link object files into the executable
$(EXEC): $(OBJECTS) | $(BUILD_DIR)
	$(CC) $(OBJECTS) -o $@

# Compile .c files into .o files, depend on objcache.h
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Create build directory if it doesn't exist
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Clean up generated files
clean:
	rm -rf $(BUILD_DIR)

# Generate compile_commands.json
bear:
	make clean && bear -- make

# Include dependency files
-include $(DEPS)

# Declare phony targets
.PHONY: all clean bear
