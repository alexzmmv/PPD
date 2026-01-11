#include "simulation.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <functional>

Simulation::Simulation()
    : timeStep(0.01),
      theta(0.5),
      softening(0.01),
      gravitationalConstant(1.0),
      numThreads(4),
      outputFilename("output.txt") {}

Simulation::~Simulation() {
    closeOutput();
}

void Simulation::initialize(const Config& config) {
    timeStep = config.timeStep;
    theta = config.theta;
    softening = config.softening;
    gravitationalConstant = config.gravitationalConstant;
    numThreads = config.numThreads;
    
    // Copy bodies from config
    bodies = config.bodies;
    
    std::cout << "Simulation initialized with " << bodies.size() << " bodies" << std::endl;
    std::cout << "Using " << numThreads << " threads" << std::endl;
}

void Simulation::setOutputFile(const std::string& filename) {
    outputFilename = filename;
    if (outputFile.is_open()) {
        outputFile.close();
    }
    outputFile.open(filename);
    if (!outputFile.is_open()) {
        std::cerr << "Error: Could not open output file: " << filename << std::endl;
    }
}

void Simulation::closeOutput() {
    if (outputFile.is_open()) {
        outputFile.close();
    }
}

std::vector<Body>& Simulation::getBodies() {
    return bodies;
}

void Simulation::setBodies(const std::vector<Body>& newBodies) {
    bodies = newBodies;
}

void Simulation::buildTree() {
    tree.clear();
    tree.build(bodies);
}

void Simulation::calculateForcesRange(int startIdx, int endIdx) {
    // This function calculates forces for bodies[startIdx] to bodies[endIdx-1]
    // Can be called by threads or MPI workers
    tree.calculateForces(bodies, startIdx, endIdx, theta, gravitationalConstant, softening);
}

void Simulation::updateBodiesRange(int startIdx, int endIdx) {
    // Update positions and velocities for a range of bodies
    // Uses leapfrog integration (velocity Verlet)
    for (int i = startIdx; i < endIdx; i++) {
        bodies[i].updateAcceleration();
        bodies[i].updateVelocity(timeStep);
        bodies[i].updatePosition(timeStep);
    }
}

void Simulation::threadWorker(int threadId, int totalThreads) {
    int numBodies = static_cast<int>(bodies.size());
    
    // Calculate range for this thread
    int bodiesPerThread = numBodies / totalThreads;
    int remainder = numBodies % totalThreads;
    
    int startIdx = threadId * bodiesPerThread + std::min(threadId, remainder);
    int endIdx = startIdx + bodiesPerThread + (threadId < remainder ? 1 : 0);
    
    // Calculate forces for this range
    calculateForcesRange(startIdx, endIdx);
}

void Simulation::calculateForcesParallel() {
    if (numThreads <= 1 || bodies.size() < static_cast<size_t>(numThreads)) {
        // Serial execution
        calculateForcesRange(0, static_cast<int>(bodies.size()));
        return;
    }
    
    std::vector<std::thread> threads;
    threads.reserve(numThreads);
    
    for (int t = 0; t < numThreads; t++) {
        threads.emplace_back(&Simulation::threadWorker, this, t, numThreads);
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
}

void Simulation::updateBodiesParallel() {
    if (numThreads <= 1 || bodies.size() < static_cast<size_t>(numThreads)) {
        // Serial execution
        updateBodiesRange(0, static_cast<int>(bodies.size()));
        return;
    }
    
    std::vector<std::thread> threads;
    threads.reserve(numThreads);
    
    int numBodies = static_cast<int>(bodies.size());
    
    for (int t = 0; t < numThreads; t++) {
        int bodiesPerThread = numBodies / numThreads;
        int remainder = numBodies % numThreads;
        
        int startIdx = t * bodiesPerThread + std::min(t, remainder);
        int endIdx = startIdx + bodiesPerThread + (t < remainder ? 1 : 0);
        
        threads.emplace_back(&Simulation::updateBodiesRange, this, startIdx, endIdx);
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
}

void Simulation::step(int stepNumber) {
    // Barnes-Hut simulation step:
    // 1. Build quadtree (serial - could be parallelized in future)
    buildTree();
    
    // 2. Calculate forces (parallel)
    calculateForcesParallel();
    
    // 3. Update positions and velocities (parallel)
    updateBodiesParallel();
    
    // 4. Write state to output file
    writeState(stepNumber);
}

void Simulation::run(int numSteps) {
    std::cout << "Starting simulation for " << numSteps << " steps..." << std::endl;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Write initial state
    writeState(0);
    
    for (int s = 1; s <= numSteps; s++) {
        step(s);
        
        // Progress output every 10% or every 100 steps for small simulations
        if (numSteps >= 10 && s % (numSteps / 10) == 0) {
            std::cout << "Progress: " << (s * 100 / numSteps) << "% (step " << s << ")" << std::endl;
        }
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    std::cout << "Simulation completed in " << duration.count() << " ms" << std::endl;
    std::cout << "Average time per step: " << (duration.count() / static_cast<double>(numSteps)) << " ms" << std::endl;
}

void Simulation::writeState(int stepNumber) {
    if (!outputFile.is_open()) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(outputMutex);
    
    outputFile << "step " << stepNumber << std::endl;
    
    for (const auto& body : bodies) {
        outputFile << body.id << " " 
                   << std::fixed << std::setprecision(6) 
                   << body.position.x << " " << body.position.y << std::endl;
    }
    
    // Empty line to separate steps (makes parsing easier)
    outputFile << std::endl;
}
