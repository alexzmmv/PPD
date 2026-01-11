#ifndef CONFIG_H
#define CONFIG_H

#include "body.h"
#include <string>
#include <vector>

class Config {
public:
    // Simulation parameters
    double timeStep;
    int numSteps;
    double theta;           // Barnes-Hut opening angle
    double softening;       // Softening parameter
    double gravitationalConstant;

    // Window parameters (for future OpenGL use)
    int windowWidth;
    int windowHeight;

    // Parallel parameters
    int numThreads;

    // Bodies loaded from config
    std::vector<Body> bodies;

    Config();

    // Load configuration from file
    bool loadFromFile(const std::string& filename);

    // Print configuration (for debugging)
    void print() const;

private:
    // Helper to trim whitespace
    std::string trim(const std::string& str) const;
    
    // Parse a key-value pair
    void parseKeyValue(const std::string& key, const std::string& value);
    
    // Parse a body definition line
    bool parseBody(const std::string& line);
};

#endif // CONFIG_H
