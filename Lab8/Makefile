CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -pthread

SRC := src/main.cpp src/dsm.cpp
INC := -Isrc

BIN := dsm_lab8

all: $(BIN)

$(BIN): $(SRC)
	$(CXX) $(CXXFLAGS) $(INC) -o $@ $(SRC)

clean:
	rm -f $(BIN)

.PHONY: all clean
