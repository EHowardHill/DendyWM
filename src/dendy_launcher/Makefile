# Makefile for Raylib App Launcher

CXX       := g++
CXXFLAGS  := -std=c++17 -Wall -Wextra -O2 \
              -I./include
LDFLAGS   := -L./lib
LDFLAGS  += -lraylib -lm -lpthread -ldl -lrt -lX11

# Optionally embed the rpath so your app finds libraylib.so at runtime:
# LDFLAGS += -Wl,-rpath=\$$ORIGIN/raylib/lib

TARGET    := dendy_launcher
SOURCE    := dendy_launcher.cpp

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CXX) $(CXXFLAGS) $(SOURCE) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)

# debug build
debug: CXXFLAGS += -g -DDEBUG
debug: $(TARGET)

# AddressSanitizer build
asan: CXXFLAGS += -fsanitize=address -g
asan: LDFLAGS  += -fsanitize=address
asan: $(TARGET)
