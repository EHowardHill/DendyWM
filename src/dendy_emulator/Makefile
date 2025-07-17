# Makefile for the simple C++ libretro launcher

# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g # Use -O2 for release builds
LDFLAGS = -lSDL2 -lGL -ldl

# Target executable name
TARGET = dendy_emulator

# Source files
SRCS = main.cpp

# Object files
OBJS = $(SRCS:.cpp=.o)

# Default target
all: $(TARGET)

# Link the executable
$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS)

# Compile source files into object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up build files
clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean install uninstall
