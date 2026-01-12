# Updated Makefile with OpenGL support
# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -pthread
DEBUG_FLAGS = -g -DDEBUG

# OpenGL flags
OPENGL_LIBS = -lGL -lGLEW -lglfw

# Directories
SRC_DIR = src
INC_DIR = include
BUILD_DIR = build

# Target executables
TARGET = nbody_sim
MPI_TARGET = nbody_mpi
VIS_TARGET = nbody_visualizer

# Source files
SOURCES = $(SRC_DIR)/main.cpp \
          $(SRC_DIR)/body.cpp \
          $(SRC_DIR)/quadtree.cpp \
          $(SRC_DIR)/config.cpp \
          $(SRC_DIR)/simulation.cpp

# MPI source files
MPI_SOURCES = $(SRC_DIR)/main_mpi.cpp \
              $(SRC_DIR)/body.cpp \
              $(SRC_DIR)/quadtree.cpp \
              $(SRC_DIR)/config.cpp \
              $(SRC_DIR)/simulation.cpp

# Visualizer source files (Vec2 is header-only, so no vec2.cpp needed)
VIS_SOURCES = $(SRC_DIR)/main_visualizer.cpp \
              $(SRC_DIR)/visualizer.cpp \
              $(SRC_DIR)/body.cpp

# Object files
OBJECTS = $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SOURCES))
MPI_OBJECTS = $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%_mpi.o,$(MPI_SOURCES))
VIS_OBJECTS = $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%_vis.o,$(VIS_SOURCES))

# Add MPI build flags
MPICXX = mpic++
MPICXXFLAGS = -std=c++17 -Wall -Wextra -O2 -I$(INC_DIR)

# Default target
all: $(BUILD_DIR) $(TARGET) $(MPI_TARGET) $(VIS_TARGET)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Build standard executable
$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Build MPI executable
$(MPI_TARGET): $(MPI_OBJECTS)
	$(MPICXX) $(MPICXXFLAGS) -o $@ $^

# Build visualizer executable
$(VIS_TARGET): $(VIS_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(OPENGL_LIBS)

# Compile standard source files to object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I$(INC_DIR) -c $< -o $@

# Compile MPI source files to object files (with _mpi suffix to avoid conflicts)
$(BUILD_DIR)/%_mpi.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(MPICXX) $(MPICXXFLAGS) -c $< -o $@

# Compile visualizer source files to object files (with _vis suffix to avoid conflicts)
$(BUILD_DIR)/%_vis.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I$(INC_DIR) -c $< -o $@

# Debug build
debug: CXXFLAGS += $(DEBUG_FLAGS)
debug: MPICXXFLAGS += $(DEBUG_FLAGS)
debug: clean all

# Run the standard simulation
run: $(TARGET)
	./$(TARGET)

# Run the MPI simulation
run-mpi: $(MPI_TARGET)
	mpirun -np 4 ./$(MPI_TARGET)

# Run the visualizer
run-vis: $(VIS_TARGET)
	./$(VIS_TARGET)

# Run complete demo: both simulations + visualization
demo: $(TARGET) $(MPI_TARGET) $(VIS_TARGET)
	@echo "Running threaded simulation..."
	./$(TARGET) config.txt output_thr.txt
	@echo "Running MPI simulation..."
	mpirun -np 4 ./$(MPI_TARGET) config.txt output_mpi.txt
	@echo "Starting visualization..."
	./$(VIS_TARGET) output_thr.txt output_mpi.txt

# Run with custom config
run-custom: $(TARGET)
	./$(TARGET) $(CONFIG) $(OUTPUT)

# Run MPI with custom config
run-mpi-custom: $(MPI_TARGET)
	mpirun -np 4 ./$(MPI_TARGET) $(CONFIG) $(OUTPUT)

# Clean build files
clean:
	rm -rf $(BUILD_DIR) $(TARGET) $(MPI_TARGET) $(VIS_TARGET)

# Clean all generated files including output
cleanall: clean
	rm -f output*.txt

# Generate dependencies (for development)
depend: $(SOURCES) $(MPI_SOURCES) $(VIS_SOURCES)
	$(CXX) $(CXXFLAGS) -I$(INC_DIR) -MM $(SOURCES) > .depend
	$(MPICXX) $(MPICXXFLAGS) -MM $(MPI_SOURCES) >> .depend
	$(CXX) $(CXXFLAGS) -I$(INC_DIR) -MM $(VIS_SOURCES) >> .depend

# Individual targets
serial: $(BUILD_DIR) $(TARGET)
mpi: $(BUILD_DIR) $(MPI_TARGET)
visualizer: $(BUILD_DIR) $(VIS_TARGET)

# Install OpenGL dependencies (Ubuntu/Debian)
install-deps:
	sudo apt-get update
	sudo apt-get install libgl1-mesa-dev libglew-dev libglfw3-dev

# Phony targets
.PHONY: all clean cleanall debug run run-mpi run-vis run-custom run-mpi-custom depend serial mpi visualizer demo install-deps

# Include dependencies if they exist
-include .depend
