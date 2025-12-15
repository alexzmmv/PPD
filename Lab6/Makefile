CXX = g++
CXXFLAGS = -std=c++17 -pthread -O2 -Wall
JAVAC = javac
JAVA = java

# Targets
CPP_TARGET = hamiltonian
JAVA_CLASS = HamiltonianJava

all: cpp java

cpp: $(CPP_TARGET)

$(CPP_TARGET): hamiltonian.cpp
	$(CXX) $(CXXFLAGS) -o $(CPP_TARGET) hamiltonian.cpp

java: $(JAVA_CLASS).class

$(JAVA_CLASS).class: $(JAVA_CLASS).java
	$(JAVAC) $(JAVA_CLASS).java

run-cpp: cpp
	./$(CPP_TARGET) 8 7 0

run-java: java
	$(JAVA) $(JAVA_CLASS) 8 7 0

test-cpp: cpp
	@echo "=== Testing C++ with Complete Graph (7 vertices, 8 threads) ==="
	./$(CPP_TARGET) 8 7 0
	@echo ""
	@echo "=== Testing C++ with Non-Cyclic Graph (6 vertices, 4 threads) ==="
	./$(CPP_TARGET) 4 6 1
	@echo ""
	@echo "=== Testing C++ with Random Graph (8 vertices, 8 threads) ==="
	./$(CPP_TARGET) 8 8 2 100
	@echo "=== Testing C++ with Random Graph (1000 vertices, 8 threads) ==="
	./$(CPP_TARGET) 8 1000 2 100

test-java: java
	@echo "=== Testing Java with Complete Graph (7 vertices, 8 threads) ==="
	$(JAVA) $(JAVA_CLASS) 8 7 0
	@echo ""
	@echo "=== Testing Java with Non-Cyclic Graph (6 vertices, 4 threads) ==="
	$(JAVA) $(JAVA_CLASS) 4 6 1
	@echo ""
	@echo "=== Testing Java with Random Graph (8 vertices, 8 threads) ==="
	$(JAVA) $(JAVA_CLASS) 8 8 2 100
	@echo "=== Testing Java with Random Graph (1000 vertices, 8 threads) ==="
	$(JAVA) $(JAVA_CLASS) 8 1000 2 100

test-all: test-cpp test-java

clean:
	rm -f $(CPP_TARGET) *.class

.PHONY: all cpp java run-cpp run-java test-cpp test-java test-all clean
