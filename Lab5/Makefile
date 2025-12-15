CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -pthread
TARGET = polynomial

all: $(TARGET)

$(TARGET): main.cpp
	$(CXX) $(CXXFLAGS) main.cpp -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all run clean
