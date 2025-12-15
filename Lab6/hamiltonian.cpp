#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <chrono>

using namespace std;

class Graph {
private:
    int n; 
    vector<vector<int>> adj; // adjacency list
    
public:
    Graph(int vertices) : n(vertices), adj(vertices) {}
    
    void addEdge(int u, int v) {
        adj[u].push_back(v);
    }
    
    int getVertexCount() const { return n; }
    
    const vector<int>& getNeighbors(int vertex) const {
        return adj[vertex];
    }
    
    void print() const {
        cout << "Graph with " << n << " vertices:\n";
        if (n > 20) {
            cout << "(Graph too large to display)\n";
            return;
        }
        for (int i = 0; i < n; i++) {
            cout << i << " - ";
            for (int neighbor : adj[i]) {
                cout << neighbor << " ";
            }
            cout << "\n";
        }
    }
};

class HamiltonianCycleFinder {
private:
    const Graph& graph;
    int numThreads;
    atomic<bool> found;
    mutex resultMutex;
    vector<int> result;
    int startVertex;
    
    bool isInPath(const vector<int>& path, int vertex) {
        return find(path.begin(), path.end(), vertex) != path.end();
    }
    
    bool isHamiltonianCycle(const vector<int>& path) {
        if (path.size() != graph.getVertexCount()) {
            return false;
        }
        
        // Check if there's an edge from last vertex back to start
        const vector<int>& neighbors = graph.getNeighbors(path.back());
        return find(neighbors.begin(), neighbors.end(), startVertex) != neighbors.end();
    }
    
    // Sequential search from a given path
    void sequentialSearch(vector<int> path) {
        if (found.load()) return;
        
        // If we've visited all vertices, check if we can return to start
        if (path.size() == graph.getVertexCount()) {
            if (isHamiltonianCycle(path)) {
                lock_guard<mutex> lock(resultMutex);
                if (!found.load()) {
                    found.store(true);
                    result = path;
                    result.push_back(startVertex);
                }
            }
            return;
        }
        
        int currentVertex = path.back();
        const vector<int>& neighbors = graph.getNeighbors(currentVertex);
        
        for (int neighbor : neighbors) {
            if (found.load()) return;
            
            if (!isInPath(path, neighbor)) {
                vector<int> newPath = path;
                newPath.push_back(neighbor);
                sequentialSearch(newPath);
            }
        }
    }
    
    // Parallel search - distributes work among threads
    void parallelSearch(vector<int> path, int threadsAvailable) {
        if (found.load()) return;
        
        if (path.size() == graph.getVertexCount()) {
            if (isHamiltonianCycle(path)) {
                lock_guard<mutex> lock(resultMutex);
                if (!found.load()) {
                    found.store(true);
                    result = path;
                    result.push_back(startVertex);
                }
            }
            return;
        }
        
        int currentVertex = path.back();
        const vector<int>& neighbors = graph.getNeighbors(currentVertex);
        
        // Filter out already visited neighbors
        vector<int> validNeighbors;
        for (int neighbor : neighbors) {
            if (!isInPath(path, neighbor)) {
                validNeighbors.push_back(neighbor);
            }
        }
        
        if (validNeighbors.empty()) return;
        
        if (threadsAvailable <= 1 || validNeighbors.size() == 1) {
            for (int neighbor : validNeighbors) {
                if (found.load()) return;
                vector<int> newPath = path;
                newPath.push_back(neighbor);
                sequentialSearch(newPath);
            }
            return;
        }
        
        int branchCount = validNeighbors.size();
        vector<int> threadAllocation(branchCount);
        int threadsPerBranch = threadsAvailable / branchCount;
        int remainingThreads = threadsAvailable % branchCount;
        
        for (int i = 0; i < branchCount; i++) {
            threadAllocation[i] = threadsPerBranch;
            if (i < remainingThreads) {
                threadAllocation[i]++;
            }
        }
        
        vector<thread> threads;
        for (int i = 0; i < branchCount; i++) {
            if (found.load()) break;
            
            int neighbor = validNeighbors[i];
            vector<int> newPath = path;
            newPath.push_back(neighbor);
            
            threads.emplace_back([this, newPath, i, &threadAllocation]() {
                if (threadAllocation[i] > 1) {
                    parallelSearch(newPath, threadAllocation[i]);
                } else {
                    sequentialSearch(newPath);
                }
            });
        }
        
        // Wait for all threads to complete
        for (auto& t : threads) {
            if (t.joinable()) {
                t.join();
            }
        }
    }
    
public:
    HamiltonianCycleFinder(const Graph& g, int threads, int start = 0) 
        : graph(g), numThreads(threads), found(false), startVertex(start) {}
    
    vector<int> findHamiltonianCycle() {
        found.store(false);
        result.clear();
        
        vector<int> initialPath = {startVertex};
        
        auto startTime = chrono::high_resolution_clock::now();
        
        parallelSearch(initialPath, numThreads);
        
        auto endTime = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::milliseconds>(endTime - startTime);
        
        cout << "Search completed in " << duration.count() << " ms\n";
        
        return result;
    }
};

Graph createCompleteGraph(int n) {
    Graph g(n);
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (i != j) {
                g.addEdge(i, j);
            }
        }
    }
    return g;
}

Graph createNonCyclicGraph(int n) {
    Graph g(n);
    for (int i = 0; i < n - 1; i++) {
        g.addEdge(i, i + 1);
        g.addEdge(i + 1, i);
    }
    return g;
}

Graph createRandomGraphDeterministic(int n, int seed = 100) {
    Graph g(n);
    srand(seed);
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (i != j && (rand() % 2 == 0)) {
                g.addEdge(i, j);
            }
        }
    }
    return g;
}

void printCycle(const vector<int>& cycle) {
    if (cycle.empty()) {
        cout << "No Hamiltonian cycle found.\n";
        return;
    }
    if (cycle.size() > 10) {
        cout << "Hamiltonian cycle found with " << cycle.size() - 1 << " vertices (not displayed due to length).\n";
        return;
    }
    cout << "Hamiltonian cycle found: ";
    for (size_t i = 0; i < cycle.size(); i++) {
        cout << cycle[i];
        if (i < cycle.size() - 1) cout << " -> ";
    }
    cout << "\n";
}

int main(int argc, char* argv[]) {
    int numThreads;
    int numVertices;
    if (argc < 4 || argc > 5) 
        return 1;  

    numThreads = atoi(argv[1]);
    numVertices = atoi(argv[2]);
    int graphType = atoi(argv[3]);
    int seed = 100;
    if (graphType == 2 && argc == 5) {
        seed = atoi(argv[4]);
    }
    if (numThreads <= 0 || numVertices <= 0 || (graphType < 0 || graphType > 2)) {
        cout << "Usage: " << argv[0] << " <num_threads> <num_vertices> <graph_type> [seed for type 2]\n";
        cout << "graph_type: 0 for complete graph, 1 for non-cyclic graph, 2 for random graph\n";
        return 1;
    }

    
    cout << "=== Hamiltonian Cycle Finder (C++ Threads) ===\n";
    cout << "Using " << numThreads << " threads in a "<< numVertices << "-vertex complete graph.\n\n";
    
    Graph graph(0);
    if (graphType == 0) {
        graph = createCompleteGraph(numVertices);
    } else if (graphType == 1) {
        graph = createNonCyclicGraph(numVertices);
    } else {
        graph = createRandomGraphDeterministic(numVertices, seed);
    }

    graph.print();
    cout << "\n";
    
    HamiltonianCycleFinder finder(graph, numThreads, 0);
    vector<int> cycle = finder.findHamiltonianCycle();
    
    printCycle(cycle);
    
    return 0;
}
