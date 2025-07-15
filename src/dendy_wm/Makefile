# Compiler and flags
CC = gcc
# FIX: Add -I. to include the current directory in the header search path
CFLAGS = -std=c99 -Wall -Wextra -Wno-unused-parameter -g -I. $(shell pkg-config --cflags wlroots)

# Use pkg-config to get linker libraries for wlroots and its dependencies
LDLIBS = $(shell pkg-config --libs wlroots)

# Source and object files
OBJECTS = dendy_wm.o
TARGET = dendy_wm

.PHONY: all clean

all: $(TARGET)

# Rule to link the final executable
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDLIBS)

# Rule to compile source files
dendy_wm.o: dendy_wm.c xdg-shell-protocol.h
	$(CC) $(CFLAGS) -c dendy_wm.c -o dendy_wm.o

# Rule to generate the Wayland protocol header
xdg-shell-protocol.h: /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml
	wayland-scanner server-header $< $@

clean:
	rm -f $(TARGET) $(OBJECTS) xdg-shell-protocol.h