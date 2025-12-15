CXX = g++
CXXFLAGS = -std=c++17 -Wall -pthread -O3
TARGET = http_downloader

UNAME := $(shell uname)

ifeq ($(UNAME), Darwin)
	LDFLAGS = -pthread
else ifeq ($(UNAME), Linux)
	LDFLAGS = -pthread
endif

all: $(TARGET)

$(TARGET): main.cpp
	$(CXX) $(CXXFLAGS) -o $(TARGET) main.cpp $(LDFLAGS)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
