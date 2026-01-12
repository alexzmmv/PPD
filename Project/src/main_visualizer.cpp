// File: Project/src/main_visualizer.cpp
#include "visualizer.h"
#include <iostream>
#include <string>

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [threaded_output] [mpi_output]" << std::endl;
    std::cout << "  threaded_output: Output file from threaded simulation (default: output.txt)" << std::endl;
    std::cout << "  mpi_output: Output file from MPI simulation (default: output_mpi.txt)" << std::endl;
    std::cout << std::endl;
    std::cout << "Example: " << programName << " output_thr.txt output_mpi.txt" << std::endl;
}

int main(int argc, char* argv[]) {
    std::string threadedFile = "output_thr.txt";
    std::string mpiFile = "output_mpi.txt";
    
    // Parse command line arguments
    if (argc >= 2) {
        std::string arg = argv[1];
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        }
        threadedFile = arg;
    }
    if (argc >= 3) {
        mpiFile = argv[2];
    }
    
    std::cout << "=== N-Body Simulation Visualizer ===" << std::endl;
    std::cout << "Threaded output file: " << threadedFile << std::endl;
    std::cout << "MPI output file: " << mpiFile << std::endl;
    std::cout << std::endl;
    
    // Create and initialize visualizer
    Visualizer visualizer(1600, 800);
    
    if (!visualizer.initialize()) {
        std::cerr << "Failed to initialize visualizer" << std::endl;
        return -1;
    }
    
    // Load simulation data
    if (!visualizer.loadSimulationData(threadedFile, mpiFile)) {
        std::cerr << "Failed to load simulation data" << std::endl;
        return -1;
    }
    
    std::cout << "Visualization initialized successfully!" << std::endl;
    
    // Run visualization
    visualizer.run();
    
    std::cout << "\nVisualization closed." << std::endl;
    return 0;
}
