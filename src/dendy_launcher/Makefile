# Makefile for Raylib App Launcher

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
LDFLAGS = -lraylib -lm -lpthread -ldl -lrt -lX11

# Detect if running on Debian/Ubuntu for package management
DISTRO := $(shell lsb_release -si 2>/dev/null || echo "Unknown")

TARGET = app_launcher
SOURCE = app_launcher.cpp

.PHONY: all clean install-deps run

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CXX) $(CXXFLAGS) $(SOURCE) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)

install-deps:
	@echo "Installing dependencies for $(DISTRO)..."
	@if [ "$(DISTRO)" = "Debian" ] || [ "$(DISTRO)" = "Ubuntu" ]; then \
		sudo apt-get update && \
		sudo apt-get install -y build-essential libraylib-dev libx11-dev; \
	else \
		echo "Please install raylib and X11 development libraries manually"; \
	fi

run: $(TARGET)
	./$(TARGET)

# Alternative build with debugging symbols
debug: CXXFLAGS += -g -DDEBUG
debug: $(TARGET)

# Build with Address Sanitizer for memory debugging
asan: CXXFLAGS += -fsanitize=address -g
asan: LDFLAGS += -fsanitize=address
asan: $(TARGET)