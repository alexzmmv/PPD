#ifndef SIMULATION_H
#define SIMULATION_H

#include "body.h"
#include "quadtree.h"
#include "config.h"
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <fstream>

// Simulation class - manages the n-body simulation with Barnes-Hut algorithm
// Designed to be scalable for MPI:
// - Force calculation is separated and works on index ranges
// - Position/velocity updates can be done independently per body
// - Tree building is centralized but could be distributed in MPI version
class Simulation {
public:
    // Simulation parameters
    double timeStep;
    double theta;
    double softening;
    double gravitationalConstant;
    int numThreads;

    // Bodies in the simulation
    std::vector<Body> bodies;

    // Quadtree for Barnes-Hut
    QuadTree tree;

    // Output file
    std::string outputFilename;
    std::ofstream outputFile;

    Simulation();
    ~Simulation();

    // Initialize simulation from config
    void initialize(const Config& config);

    // Set output file for body positions
    void setOutputFile(const std::string& filename);

    // Run simulation for specified number of steps
    void run(int numSteps);

    // Run a single simulation step
    void step(int stepNumber);

    // Write current state to output file
    void writeState(int stepNumber);

    // Close output file
    void closeOutput();

    // Get bodies (for external access, e.g., MPI communication)
    std::vector<Body>& getBodies();

    // Set bodies (for external updates, e.g., from MPI)
    void setBodies(const std::vector<Body>& newBodies);

    // --- Methods designed for parallel/MPI scalability ---
    
    // Build the quadtree from current body positions
    void buildTree();

    // Calculate forces for a range of bodies (thread/MPI worker function)
    void calculateForcesRange(int startIdx, int endIdx);

    // Update positions and velocities for a range of bodies
    void updateBodiesRange(int startIdx, int endIdx);

private:
    // Worker function for threaded force calculation
    void threadWorker(int threadId, int totalThreads);

    // Parallel force calculation using threads
    void calculateForcesParallel();

    // Parallel position/velocity update using threads
    void updateBodiesParallel();

    std::mutex outputMutex;
};

#endif // SIMULATION_H
