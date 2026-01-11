# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -pthread
DEBUG_FLAGS = -g -DDEBUG

# Directories
SRC_DIR = src
INC_DIR = include
BUILD_DIR = build

# Target executable
TARGET = nbody_sim

# Source files
SOURCES = $(SRC_DIR)/main.cpp \
          $(SRC_DIR)/body.cpp \
          $(SRC_DIR)/quadtree.cpp \
          $(SRC_DIR)/config.cpp \
          $(SRC_DIR)/simulation.cpp

# Object files
OBJECTS = $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SOURCES))

# Default target
all: $(BUILD_DIR) $(TARGET)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Link object files to create executable
$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Compile source files to object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I$(INC_DIR) -c $< -o $@

# Debug build
debug: CXXFLAGS += $(DEBUG_FLAGS)
debug: clean all

# Run the simulation
run: $(TARGET)
	./$(TARGET)

# Run with custom config
run-custom: $(TARGET)
	./$(TARGET) $(CONFIG) $(OUTPUT)

# Clean build files
clean:
	rm -rf $(BUILD_DIR) $(TARGET)

# Clean all generated files including output
cleanall: clean
	rm -f output.txt

# Generate dependencies (for development)
depend: $(SOURCES)
	$(CXX) $(CXXFLAGS) -I$(INC_DIR) -MM $^ > .depend

# Phony targets
.PHONY: all clean cleanall debug run run-custom depend

# Include dependencies if they exist
-include .depend
