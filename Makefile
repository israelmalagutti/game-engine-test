# Compiler
CXX = g++

# Compiler flags
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic -I./include

# Libraries
LIBS = -lSDL2 -lSDL2_image -lGL

# Find all source files
SOURCES = $(wildcard src/*.cpp) src/glad.c

# Output binary
TARGET = game

# Default target
all: $(TARGET)

# Build the game
$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET) $(LIBS)

# Clean build files
clean:
	rm -f $(TARGET)

# Build and run
run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
