CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -Wno-unused-parameter -I. -I/usr/include/libdrm -I/usr/include/pixman-1
OBJECTS = dendy_wm.o

all: dendy_wm

dendy_wm: $(OBJECTS)
	$(CC) $(OBJECTS) -o dendy_wm -lwayland-server -lwlroots -lxkbcommon

dendy_wm.o: dendy_wm.c xdg-shell-protocol.h
	$(CC) $(CFLAGS) -c dendy_wm.c -o dendy_wm.o

xdg-shell-protocol.h:
	wayland-scanner server-header /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml xdg-shell-protocol.h

clean:
	rm -f dendy_wm $(OBJECTS) xdg-shell-protocol.h