#include "config.h"
#include "simulation.h"
#include <iostream>
#include <string>

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [config_file] [output_file]" << std::endl;
    std::cout << "  config_file: Path to configuration file (default: config.txt)" << std::endl;
    std::cout << "  output_file: Path to output file (default: output.txt)" << std::endl;
}

int main(int argc, char* argv[]) {
    std::string configFile = "config.txt";
    std::string outputFile = "output.txt";
    
    // Parse command line arguments
    if (argc >= 2) {
        std::string arg = argv[1];
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        }
        configFile = arg;
    }
    if (argc >= 3) {
        outputFile = argv[2];
    }
    
    std::cout << "=== N-Body Barnes-Hut Simulation ===" << std::endl;
    std::cout << "Config file: " << configFile << std::endl;
    std::cout << "Output file: " << outputFile << std::endl;
    std::cout << std::endl;
    
    // Load configuration
    Config config;
    if (!config.loadFromFile(configFile)) {
        std::cerr << "Failed to load configuration from " << configFile << std::endl;
        return 1;
    }
    
    // Print configuration
    config.print();
    std::cout << std::endl;
    
    // Check if we have bodies
    if (config.bodies.empty()) {
        std::cerr << "Error: No bodies defined in configuration file" << std::endl;
        return 1;
    }
    
    // Initialize simulation
    Simulation simulation;
    simulation.initialize(config);
    simulation.setOutputFile(outputFile);
    
    // Run simulation
    simulation.run(config.numSteps);

    // Close output
    simulation.closeOutput();
    
    std::cout << std::endl;
    std::cout << "Output written to: " << outputFile << std::endl;
    std::cout << "=== Simulation Complete ===" << std::endl;
    
    return 0;
}
