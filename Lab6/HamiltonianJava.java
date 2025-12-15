import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicBoolean;

class Graph {
    private int n;
    private List<List<Integer>> adj;
    
    public Graph(int vertices) {
        this.n = vertices;
        this.adj = new ArrayList<>(vertices);
        for (int i = 0; i < vertices; i++) {
            adj.add(new ArrayList<>());
        }
    }
    
    public void addEdge(int u, int v) {
        adj.get(u).add(v);
    }
    
    public int getVertexCount() {
        return n;
    }
    
    public List<Integer> getNeighbors(int vertex) {
        return adj.get(vertex);
    }
    
    public void print() {
        System.out.println("Graph with " + n + " vertices:");
        if (n > 20) {
            System.out.println("(Graph too large to display)");
            return;
        }
        for (int i = 0; i < n; i++) {
            System.out.print(i + " - ");
            for (int neighbor : adj.get(i)) {
                System.out.print(neighbor + " ");
            }
            System.out.println();
        }
    }
}

class HamiltonianSearchTask extends RecursiveTask<List<Integer>> {
    private final Graph graph;
    private final List<Integer> path;
    private final int startVertex;
    private final int threadsAvailable;
    private final AtomicBoolean found;
    private final int threshold;
    
    public HamiltonianSearchTask(Graph graph, List<Integer> path, int startVertex, 
                                  int threadsAvailable, AtomicBoolean found, int threshold) {
        this.graph = graph;
        this.path = new ArrayList<>(path);
        this.startVertex = startVertex;
        this.threadsAvailable = threadsAvailable;
        this.found = found;
        this.threshold = threshold;
    }
    
    private boolean isInPath(List<Integer> path, int vertex) {
        return path.contains(vertex);
    }
    
    private boolean isHamiltonianCycle(List<Integer> path) {
        if (path.size() != graph.getVertexCount()) {
            return false;
        }
        
        List<Integer> neighbors = graph.getNeighbors(path.get(path.size() - 1));
        return neighbors.contains(startVertex);
    }
    
    private List<Integer> sequentialSearch(List<Integer> currentPath) {
        if (found.get()) {
            return Collections.emptyList();
        }
        
        if (currentPath.size() == graph.getVertexCount()) {
            if (isHamiltonianCycle(currentPath)) {
                if (found.compareAndSet(false, true)) {
                    List<Integer> result = new ArrayList<>(currentPath);
                    result.add(startVertex);
                    return result;
                }
            }
            return Collections.emptyList();
        }
        
        int currentVertex = currentPath.get(currentPath.size() - 1);
        List<Integer> neighbors = graph.getNeighbors(currentVertex);
        
        for (int neighbor : neighbors) {
            if (found.get()) {
                return Collections.emptyList();
            }
            
            if (!isInPath(currentPath, neighbor)) {
                List<Integer> newPath = new ArrayList<>(currentPath);
                newPath.add(neighbor);
                List<Integer> result = sequentialSearch(newPath);
                if (!result.isEmpty()) {
                    return result;
                }
            }
        }
        
        return Collections.emptyList();
    }
    
    @Override
    protected List<Integer> compute() {
        if (found.get()) {
            return Collections.emptyList();
        }
        
        if (path.size() == graph.getVertexCount()) {
            if (isHamiltonianCycle(path)) {
                if (found.compareAndSet(false, true)) {
                    List<Integer> result = new ArrayList<>(path);
                    result.add(startVertex);
                    return result;
                }
            }
            return Collections.emptyList();
        }
        
        int currentVertex = path.get(path.size() - 1);
        List<Integer> neighbors = graph.getNeighbors(currentVertex);
        
        List<Integer> validNeighbors = new ArrayList<>();
        for (int neighbor : neighbors) {
            if (!isInPath(path, neighbor)) {
                validNeighbors.add(neighbor);
            }
        }
        
        if (validNeighbors.isEmpty()) {
            return Collections.emptyList();
        }
        
        // If only one thread or at threshold, do sequential search
        if (threadsAvailable <= threshold || validNeighbors.size() == 1) {
            for (int neighbor : validNeighbors) {
                if (found.get()) {
                    return Collections.emptyList();
                }
                List<Integer> newPath = new ArrayList<>(path);
                newPath.add(neighbor);
                List<Integer> result = sequentialSearch(newPath);
                if (!result.isEmpty()) {
                    return result;
                }
            }
            return Collections.emptyList();
        }
        
        // Distribute threads among branches
        int branchCount = validNeighbors.size();
        int[] threadAllocation = new int[branchCount];
        int threadsPerBranch = threadsAvailable / branchCount;
        int remainingThreads = threadsAvailable % branchCount;
        
        for (int i = 0; i < branchCount; i++) {
            threadAllocation[i] = threadsPerBranch;
            if (i < remainingThreads) {
                threadAllocation[i]++;
            }
        }
        
        // Create subtasks
        List<HamiltonianSearchTask> tasks = new ArrayList<>();
        for (int i = 0; i < branchCount; i++) {
            if (found.get()) {
                break;
            }
            
            int neighbor = validNeighbors.get(i);
            List<Integer> newPath = new ArrayList<>(path);
            newPath.add(neighbor);
            
            HamiltonianSearchTask task = new HamiltonianSearchTask(
                graph, newPath, startVertex, threadAllocation[i], found, threshold
            );
            tasks.add(task);
        }
        
        // Fork all tasks except the last one
        for (int i = 0; i < tasks.size() - 1; i++) {
            tasks.get(i).fork();
        }
        
        // Compute the last task directly
        List<Integer> result = Collections.emptyList();
        if (!tasks.isEmpty()) {
            result = tasks.get(tasks.size() - 1).compute();
        }
        
        // Join all forked tasks
        for (int i = 0; i < tasks.size() - 1; i++) {
            if (found.get() && !result.isEmpty()) {
                break;
            }
            List<Integer> taskResult = tasks.get(i).join();
            if (!taskResult.isEmpty()) {
                result = taskResult;
            }
        }
        
        return result;
    }
}

class HamiltonianCycleFinder {
    private final Graph graph;
    private final int numThreads;
    private final int startVertex;
    
    public HamiltonianCycleFinder(Graph graph, int numThreads, int startVertex) {
        this.graph = graph;
        this.numThreads = numThreads;
        this.startVertex = startVertex;
    }
    
    public List<Integer> findHamiltonianCycle() {
        ForkJoinPool pool = new ForkJoinPool(numThreads);
        AtomicBoolean found = new AtomicBoolean(false);
        
        List<Integer> initialPath = new ArrayList<>();
        initialPath.add(startVertex);
        
        HamiltonianSearchTask task = new HamiltonianSearchTask(
            graph, initialPath, startVertex, numThreads, found, 1
        );
        
        long startTime = System.currentTimeMillis();
        
        List<Integer> result = pool.invoke(task);
        
        long endTime = System.currentTimeMillis();
        System.out.println("Search completed in " + (endTime - startTime) + " ms");
        
        pool.shutdown();
        
        return result;
    }
}

public class HamiltonianJava {
    
    private static Graph createCompleteGraph(int n) {
        Graph g = new Graph(n);
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                if (i != j) {
                    g.addEdge(i, j);
                }
            }
        }
        return g;
    }
    
    private static Graph createNonCyclicGraph(int n) {
        Graph g = new Graph(n);
        for (int i = 0; i < n - 1; i++) {
            g.addEdge(i, i + 1);
            g.addEdge(i + 1, i);
        }
        return g;
    }
    
    private static Graph createRandomGraphDeterministic(int n, int seed) {
        Graph g = new Graph(n);
        java.util.Random rand = new java.util.Random(seed);
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                if (i != j && rand.nextInt(2) == 0) {
                    g.addEdge(i, j);
                }
            }
        }
        return g;
    }
    
    private static void printCycle(List<Integer> cycle) {
        if (cycle.isEmpty()) {
            System.out.println("No Hamiltonian cycle found.");
            return;
        }
        if (cycle.size() > 10) {
            System.out.println("Hamiltonian cycle found with " + (cycle.size() - 1) + " vertices (not displayed due to length).");
            return;
        }
        System.out.print("Hamiltonian cycle found: ");
        for (int i = 0; i < cycle.size(); i++) {
            System.out.print(cycle.get(i));
            if (i < cycle.size() - 1) {
                System.out.print(" -> ");
            }
        }
        System.out.println();
    }
    
    public static void main(String[] args) {
        int numThreads;
        int numVertices;
        
        if (args.length < 3 || args.length > 4) {
            return;
        }
        
        numThreads = Integer.parseInt(args[0]);
        numVertices = Integer.parseInt(args[1]);
        int graphType = Integer.parseInt(args[2]);
        int seed = 100;
        if (graphType == 2 && args.length == 4) {
            seed = Integer.parseInt(args[3]);
        }
        
        if (numThreads <= 0 || numVertices <= 0 || (graphType < 0 || graphType > 2)) {
            System.out.println("Usage: java HamiltonianJava <num_threads> <num_vertices> <graph_type> [seed for type 2]");
            System.out.println("graph_type: 0 for complete graph, 1 for non-cyclic graph, 2 for random graph");
            return;
        }
        
        System.out.println("=== Hamiltonian Cycle Finder (Java ForkJoinPool) ===");
        System.out.println("Using " + numThreads + " threads in a " + numVertices + "-vertex complete graph.\n");
        
        Graph graph;
        if (graphType == 0) {
            graph = createCompleteGraph(numVertices);
        } else if (graphType == 1) {
            graph = createNonCyclicGraph(numVertices);
        } else {
            graph = createRandomGraphDeterministic(numVertices, seed);
        }
        
        graph.print();
        System.out.println();
        
        HamiltonianCycleFinder finder = new HamiltonianCycleFinder(graph, numThreads, 0);
        List<Integer> cycle = finder.findHamiltonianCycle();
        
        printCycle(cycle);
    }
}
