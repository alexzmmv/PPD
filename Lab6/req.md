Solve the problem below:

Given a directed graph, find a Hamiltonean cycle, if one exists. Use a specified number of threads to parallelize the search. Important The search should start from a fixed vertex (no need to take each vertex as the starting point), however, the splitting of the work between threads should happen at several levels, for all possible choices among the neighbors of each current vertex.

For example, if you have 8 threads and the first vertex has an out-degree of 3, two of the branches will have 3 threads allocated each, and the remaining branch will have 2 threads. Then, if on the first branch the neighbor of the start vertex has a 4 out-neighbors distinct from the start vertex, then two of them will be explored by one thread, and the last thread will explore the other 2 neighbors; further down, there will be non-parallel search.

Create then a second implementation in Java using ForkJoinPool and RecursiveTask.

## Build Instructions (Makefile)

Use the following commands:
- `make all` - Compile both C++ and Java implementations
- `make cpp` - Compile C++ implementation only
- `make java` - Compile Java implementation only
- `make run-cpp` - Run the C++ program with default parameters (8 threads, 7 vertices)
- `make run-java` - Run the Java program with default parameters (8 threads, 7 vertices)
- `make test-cpp` - Run comprehensive C++ tests
- `make test-java` - Run comprehensive Java tests
- `make test-all` - Run all tests
- `make clean` - Remove compiled files

