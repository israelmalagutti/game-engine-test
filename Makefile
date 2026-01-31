# Compiler settings
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra

# Find all .cpp files in src/
SOURCES = $(wildcard src/*.cpp)

# Output binary name
TARGET = game

# Build the game
all:
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET)

# Remove built files
clean:
	rm -f $(TARGET)

# Build and run
run: all
	./$(TARGET)
