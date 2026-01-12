#include <mpi.h>
#include "simulation.h"
#include "config.h"
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <chrono>

void printUsage(const char* programName) {
    if (programName)
        std::cout << "Usage: mpirun -np <num_procs> " << programName << " [config_file] [output_file]" << std::endl;
    std::cout << "  config_file: Path to configuration file (default: config.txt)" << std::endl;
    std::cout << "  output_file: Path to output file (default: output.txt)" << std::endl;
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    std::string configFile = "config.txt";
    std::string outputFile = "output_mpi.txt";

    // Parse command line arguments (only rank 0 needs to do this)
    if (rank == 0) {
        if (argc >= 2) {
            std::string arg = argv[1];
            if (arg == "-h" || arg == "--help") {
                printUsage(argv[0]);
                MPI_Abort(MPI_COMM_WORLD, 0);
                return 0;
            }
            configFile = arg;
        }
        if (argc >= 3) {
            outputFile = argv[2];
        }
    }

    // Broadcast file names to all ranks
    int configLen = 0, outputLen = 0;
    if (rank == 0) {
        configLen = configFile.size();
        outputLen = outputFile.size();
    }
    MPI_Bcast(&configLen, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&outputLen, 1, MPI_INT, 0, MPI_COMM_WORLD);

    if (rank != 0) {
        configFile.resize(configLen);
        outputFile.resize(outputLen);
    }
    MPI_Bcast(configFile.data(), configLen, MPI_CHAR, 0, MPI_COMM_WORLD);
    MPI_Bcast(outputFile.data(), outputLen, MPI_CHAR, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        std::cout << "=== N-Body Barnes-Hut Simulation (MPI) ===" << std::endl;
        std::cout << "Config file: " << configFile << std::endl;
        std::cout << "Output file: " << outputFile << std::endl;
        std::cout << "MPI ranks: " << size << std::endl;
        std::cout << std::endl;
    }

    // Load and broadcast configuration
    Config config;
    bool configLoaded = true;
    if (rank == 0) {
        configLoaded = config.loadFromFile(configFile);
        if (configLoaded) {
            config.print();
            std::cout << std::endl;
        }
    }

    MPI_Bcast(&configLoaded, 1, MPI_CXX_BOOL, 0, MPI_COMM_WORLD);
    if (!configLoaded) {
        if (rank == 0)
            std::cerr << "Failed to load configuration from " << configFile << std::endl;
        MPI_Finalize();
        return 1;
    }

    // Broadcast config parameters to all ranks
    MPI_Bcast(&config.timeStep, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Bcast(&config.theta, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Bcast(&config.softening, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Bcast(&config.gravitationalConstant, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Bcast(&config.numSteps, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&config.numThreads, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // Broadcast bodies using simple serialization
    int numBodies = (rank == 0) ? config.bodies.size() : 0;
    MPI_Bcast(&numBodies, 1, MPI_INT, 0, MPI_COMM_WORLD);

    if (numBodies == 0) {
        if (rank == 0)
            std::cerr << "Error: No bodies defined in configuration file" << std::endl;
        MPI_Finalize();
        return 1;
    }

    // Use simple data array for broadcasting bodies
    std::vector<double> bodyData(numBodies * 8); // id, mass, px, py, vx, vy, ax, ay

    if (rank == 0) {
        for (int i = 0; i < numBodies; ++i) {
            bodyData[i * 8 + 0] = config.bodies[i].id;
            bodyData[i * 8 + 1] = config.bodies[i].mass;
            bodyData[i * 8 + 2] = config.bodies[i].position.x;
            bodyData[i * 8 + 3] = config.bodies[i].position.y;
            bodyData[i * 8 + 4] = config.bodies[i].velocity.x;
            bodyData[i * 8 + 5] = config.bodies[i].velocity.y;
            bodyData[i * 8 + 6] = config.bodies[i].acceleration.x;
            bodyData[i * 8 + 7] = config.bodies[i].acceleration.y;
        }
    }

    MPI_Bcast(bodyData.data(), numBodies * 8, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    if (rank != 0) {
        config.bodies.resize(numBodies);
        for (int i = 0; i < numBodies; ++i) {
            config.bodies[i].id = static_cast<int>(bodyData[i * 8 + 0]);
            config.bodies[i].mass = bodyData[i * 8 + 1];
            config.bodies[i].position.x = bodyData[i * 8 + 2];
            config.bodies[i].position.y = bodyData[i * 8 + 3];
            config.bodies[i].velocity.x = bodyData[i * 8 + 4];
            config.bodies[i].velocity.y = bodyData[i * 8 + 5];
            config.bodies[i].acceleration.x = bodyData[i * 8 + 6];
            config.bodies[i].acceleration.y = bodyData[i * 8 + 7];
        }
    }

    if (rank == 0) {
        if (config.bodies.empty()) {
            std::cerr << "Error: No bodies defined in configuration file" << std::endl;
        }
    }

    // Check for empty bodies on all ranks
    bool hasBodies = !config.bodies.empty();
    int allHaveBodies = 0;
    MPI_Allreduce(&hasBodies, &allHaveBodies, 1, MPI_INT, MPI_LAND, MPI_COMM_WORLD);
    if (!allHaveBodies) {
        MPI_Finalize();
        return 1;
    }

    // Initialize simulation on all ranks
    Simulation sim;
    sim.initialize(config);

    // Only rank 0 handles file output
    if (rank == 0) {
        sim.setOutputFile(outputFile);
    }

    // Calculate work distribution
    int bodiesPerRank = numBodies / size;
    int remainder = numBodies % size;
    int startIdx = rank * bodiesPerRank + std::min(rank, remainder);
    int endIdx = startIdx + bodiesPerRank + (rank < remainder ? 1 : 0);
    int localNumBodies = endIdx - startIdx;

    if (rank == 0) {
        std::cout << "Starting simulation for " << config.numSteps << " steps..." << std::endl;
        std::cout << "MPI Configuration:" << std::endl;
        std::cout << "  Total MPI ranks: " << size << std::endl;
        std::cout << "  Bodies per rank distribution:" << std::endl;
        for (int r = 0; r < size; r++) {
            int rStart = r * bodiesPerRank + std::min(r, remainder);
            int rEnd = rStart + bodiesPerRank + (r < remainder ? 1 : 0);
            std::cout << "    Rank " << r << ": bodies " << rStart << "-" << (rEnd - 1)
                << " (" << (rEnd - rStart) << " bodies)" << std::endl;
        }
        std::cout << std::endl;
    }

    // Start timing
    auto startTime = std::chrono::high_resolution_clock::now();

    // Main simulation loop
    for (int step = 0; step <= config.numSteps; step++) {
        auto& bodies = sim.getBodies();

        // Synchronize body data across all ranks
        if (rank == 0) {
            // Pack data from bodies to bodyData
            for (int i = 0; i < numBodies; ++i) {
                bodyData[i * 8 + 0] = bodies[i].id;
                bodyData[i * 8 + 1] = bodies[i].mass;
                bodyData[i * 8 + 2] = bodies[i].position.x;
                bodyData[i * 8 + 3] = bodies[i].position.y;
                bodyData[i * 8 + 4] = bodies[i].velocity.x;
                bodyData[i * 8 + 5] = bodies[i].velocity.y;
                bodyData[i * 8 + 6] = bodies[i].acceleration.x;
                bodyData[i * 8 + 7] = bodies[i].acceleration.y;
            }
        }

        // Broadcast current state to all ranks
        MPI_Bcast(bodyData.data(), numBodies * 8, MPI_DOUBLE, 0, MPI_COMM_WORLD);

        // Unpack data on all ranks
        for (int i = 0; i < numBodies; ++i) {
            bodies[i].id = static_cast<int>(bodyData[i * 8 + 0]);
            bodies[i].mass = bodyData[i * 8 + 1];
            bodies[i].position.x = bodyData[i * 8 + 2];
            bodies[i].position.y = bodyData[i * 8 + 3];
            bodies[i].velocity.x = bodyData[i * 8 + 4];
            bodies[i].velocity.y = bodyData[i * 8 + 5];
            bodies[i].acceleration.x = bodyData[i * 8 + 6];
            bodies[i].acceleration.y = bodyData[i * 8 + 7];
        }

        // Only rank 0 writes output for this step
        if (rank == 0) {
            sim.writeState(step);
        }

        // Don't perform simulation step for the last iteration (just write final state)
        if (step >= config.numSteps) {
            break;
        }

        // Build tree on all ranks (needed for force calculations)
        sim.buildTree();

        // Each rank calculates forces for its assigned bodies
        if (localNumBodies > 0) {
            sim.calculateForcesRange(startIdx, endIdx);
        }

        // Gather force results back to rank 0
        std::vector<double> localForceData(localNumBodies * 4); // ax, ay, fx, fy
        for (int i = 0; i < localNumBodies; ++i) {
            int bodyIdx = startIdx + i;
            localForceData[i * 4 + 0] = bodies[bodyIdx].acceleration.x;
            localForceData[i * 4 + 1] = bodies[bodyIdx].acceleration.y;
            localForceData[i * 4 + 2] = bodies[bodyIdx].force.x;
            localForceData[i * 4 + 3] = bodies[bodyIdx].force.y;
        }

        // Gather all force data to rank 0
        std::vector<int> recvCounts(size);
        std::vector<int> displs(size);
        for (int r = 0; r < size; r++) {
            int rStart = r * bodiesPerRank + std::min(r, remainder);
            int rEnd = rStart + bodiesPerRank + (r < remainder ? 1 : 0);
            recvCounts[r] = (rEnd - rStart) * 4;
            displs[r] = (r == 0) ? 0 : displs[r - 1] + recvCounts[r - 1];
        }

        std::vector<double> allForceData;
        if (rank == 0) {
            allForceData.resize(numBodies * 4);
        }

        MPI_Gatherv(localForceData.data(), localNumBodies * 4, MPI_DOUBLE,
            allForceData.data(), recvCounts.data(), displs.data(), MPI_DOUBLE,
            0, MPI_COMM_WORLD);

        // Rank 0 updates all body forces and accelerations
        if (rank == 0) {
            for (int r = 0; r < size; r++) {
                int rStart = r * bodiesPerRank + std::min(r, remainder);
                int rEnd = rStart + bodiesPerRank + (r < remainder ? 1 : 0);
                int dataOffset = displs[r] / 4;

                for (int i = rStart; i < rEnd; ++i) {
                    int idx = i - rStart + dataOffset;
                    bodies[i].acceleration.x = allForceData[idx * 4 + 0];
                    bodies[i].acceleration.y = allForceData[idx * 4 + 1];
                    bodies[i].force.x = allForceData[idx * 4 + 2];
                    bodies[i].force.y = allForceData[idx * 4 + 3];
                }
            }

            // Update positions and velocities for all bodies on rank 0
            sim.updateBodiesRange(0, numBodies);
        }

        // Progress indicator (matching the standard version)
        if (rank == 0) {
            if (config.numSteps >= 10 && (step + 1) % (config.numSteps / 10) == 0) {
                std::cout << "Progress: " << ((step + 1) * 100 / config.numSteps) << "% (step " << (step + 1) << ")" << std::endl;
            }
        }
    }

    // End timing
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    // Cleanup and final output (only rank 0)
    if (rank == 0) {
        sim.closeOutput();

        std::cout << "Simulation completed in " << duration.count() << " ms" << std::endl;
        std::cout << "Average time per step: " << (duration.count() / static_cast<double>(config.numSteps)) << " ms" << std::endl;
        std::cout << std::endl;
        std::cout << "Output written to: " << outputFile << std::endl;
        std::cout << "=== Simulation Complete ===" << std::endl;
    }

    MPI_Finalize();
    return 0;
}
