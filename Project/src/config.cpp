#include "config.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>

Config::Config() 
    : timeStep(0.01),
      numSteps(1000),
      theta(0.5),
      softening(0.01),
      gravitationalConstant(1.0),
      windowWidth(800),
      windowHeight(800),
      numThreads(4) {}

std::string Config::trim(const std::string& str) const {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, last - first + 1);
}

void Config::parseKeyValue(const std::string& key, const std::string& value) {
    std::string k = trim(key);
    std::string v = trim(value);
    
    // Convert key to lowercase for comparison
    std::string keyLower = k;
    std::transform(keyLower.begin(), keyLower.end(), keyLower.begin(), ::tolower);
    
    if (keyLower == "time_step" || keyLower == "timestep") {
        timeStep = std::stod(v);
    } else if (keyLower == "num_steps" || keyLower == "numsteps") {
        numSteps = std::stoi(v);
    } else if (keyLower == "theta") {
        theta = std::stod(v);
    } else if (keyLower == "softening") {
        softening = std::stod(v);
    } else if (keyLower == "gravitational_constant" || keyLower == "g") {
        gravitationalConstant = std::stod(v);
    } else if (keyLower == "window_width" || keyLower == "windowwidth") {
        windowWidth = std::stoi(v);
    } else if (keyLower == "window_height" || keyLower == "windowheight") {
        windowHeight = std::stoi(v);
    } else if (keyLower == "num_threads" || keyLower == "numthreads") {
        numThreads = std::stoi(v);
    }
}

bool Config::parseBody(const std::string& line) {
    std::istringstream iss(line);
    int id;
    double mass, x, y, vx, vy;
    
    if (iss >> id >> mass >> x >> y >> vx >> vy) {
        bodies.emplace_back(id, mass, Vec2(x, y), Vec2(vx, vy));
        return true;
    }
    return false;
}

bool Config::loadFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open config file: " << filename << std::endl;
        return false;
    }
    
    std::string line;
    bool parsingBodies = false;
    
    while (std::getline(file, line)) {
        line = trim(line);
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        // Check if we're starting to parse bodies
        if (line.find("Bodies:") != std::string::npos || 
            line.find("bodies:") != std::string::npos ||
            line.find("BODIES:") != std::string::npos) {
            parsingBodies = true;
            continue;
        }
        
        if (parsingBodies) {
            // Try to parse as body definition
            parseBody(line);
        } else {
            // Try to parse as key=value
            size_t eqPos = line.find('=');
            if (eqPos != std::string::npos) {
                std::string key = line.substr(0, eqPos);
                std::string value = line.substr(eqPos + 1);
                parseKeyValue(key, value);
            }
        }
    }
    
    file.close();
    return true;
}

void Config::print() const {
    std::cout << "=== Configuration ===" << std::endl;
    std::cout << "Time Step: " << timeStep << std::endl;
    std::cout << "Num Steps: " << numSteps << std::endl;
    std::cout << "Theta: " << theta << std::endl;
    std::cout << "Softening: " << softening << std::endl;
    std::cout << "Gravitational Constant: " << gravitationalConstant << std::endl;
    std::cout << "Window: " << windowWidth << "x" << windowHeight << std::endl;
    std::cout << "Num Threads: " << numThreads << std::endl;
    std::cout << "Bodies: " << bodies.size() << std::endl;
    
    for (const auto& body : bodies) {
        std::cout << "  Body " << body.id << ": mass=" << body.mass 
                  << " pos=" << body.position << " vel=" << body.velocity << std::endl;
    }
    std::cout << "=====================" << std::endl;
}
