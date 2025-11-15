# Compiler to use
CC = gcc

# Compiler flags
CFLAGS = -Iinclude -Wall -g -MD

# Directories
SRC_DIR = src
BUILD_DIR = build
INCLUDE_DIR = include
TEST_DIR = test

# Source files
SOURCES = $(wildcard $(SRC_DIR)/*.c)
TEST_SOURCES = $(wildcard $(TEST_DIR)/*.c)

# Object files
OBJECTS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SOURCES))
TEST_OBJECTS = $(patsubst $(TEST_DIR)/%.c, $(BUILD_DIR)/%.o, $(TEST_SOURCES))

# Dependency files
DEPS = $(OBJECTS:.o=.d)
TEST_DEPS = $(TEST_OBJECTS:.o=.d)

# Names
STATIC_LIB = $(BUILD_DIR)/objcache.a
TEST_EXEC = $(BUILD_DIR)/objcache_test

# Default target: build static library
all: $(STATIC_LIB)

# Build static library from src/ objects
$(STATIC_LIB): $(OBJECTS) | $(BUILD_DIR)
	ar rcs $@ $^

# Build test executable (library + test objects)
test: $(TEST_EXEC)

$(TEST_EXEC): $(OBJECTS) $(TEST_OBJECTS) | $(BUILD_DIR)
	$(CC) $^ -o $@

# Compile src/ .c files into .o
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile test/ .c files into .o
$(BUILD_DIR)/%.o: $(TEST_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Create build directory if it doesn't exist
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Clean up
clean:
	rm -rf $(BUILD_DIR)

# Generate compile_commands.json
bear:
	make clean && bear -- make

# Include dependency files
-include $(DEPS)
-include $(TEST_DEPS)

.PHONY: all clean bear test
