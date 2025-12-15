# Makefile for PPD Bank Accounts Project

CC = gcc
CFLAGS = -pthread -Wall -Wextra
TARGET = main
SRC = main.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)
