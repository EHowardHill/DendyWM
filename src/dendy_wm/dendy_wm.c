#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h> // For bool, true, false

// Global variables
static Display *display;
static Window root;
static Window window_zero = 0; // Will hold the ID of the initial window

// Function to start the initial application
void start_initial_app(const char *app_path)
{
    if (fork() == 0)
    {
        // We are in the child process
        // Close the display connection for the child process
        XCloseDisplay(display); 
        if (execl(app_path, app_path, (char *)NULL) == -1)
        {
            perror("execl failed");
            exit(1);
        }
    }
}

// Function to close all windows except window_zero
void close_all_other_windows()
{
    Window root_return, parent_return, *children;
    unsigned int num_children;

    // Query the window tree to get all client windows
    if (XQueryTree(display, root, &root_return, &parent_return, &children, &num_children) == 0)
    {
        return; // Failed to query
    }

    printf("Closing all windows except Window 0 (ID: %lu)...\n", window_zero);

    for (unsigned int i = 0; i < num_children; i++)
    {
        // Crucially, check if the window is NOT window_zero
        if (children[i] != window_zero)
        {
            // Send the WM_DELETE_WINDOW message to gracefully close the application
            Atom wm_delete_window = XInternAtom(display, "WM_DELETE_WINDOW", False);
            XEvent ev;
            ev.type = ClientMessage;
            ev.xclient.window = children[i];
            ev.xclient.message_type = XInternAtom(display, "WM_PROTOCOLS", True);
            ev.xclient.format = 32;
            ev.xclient.data.l[0] = wm_delete_window;
            ev.xclient.data.l[1] = CurrentTime;
            XSendEvent(display, children[i], False, NoEventMask, &ev);
            
            // For applications that don't respond to WM_DELETE_WINDOW, 
            // you might consider a more forceful XDestroyWindow(display, children[i]) after a timeout.
        }
    }

    // Free the list of children returned by XQueryTree
    if (children)
    {
        XFree(children);
    }
}

int main(int argc, char *argv[])
{
    // 1. Check for command-line argument
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s /path/to/initial/application\n", argv[0]);
        return 1;
    }

    // 2. Open connection to the X server
    if (!(display = XOpenDisplay(NULL)))
    {
        fprintf(stderr, "Cannot open display\n");
        return 1;
    }
    root = DefaultRootWindow(display);
    int screen_width = DisplayWidth(display, DefaultScreen(display));
    int screen_height = DisplayHeight(display, DefaultScreen(display));

    // 3. Become the Window Manager
    // We ask to be notified when a window wants to be mapped (shown)
    // or configured. This is the core of being a window manager.
    XSelectInput(display, root, SubstructureRedirectMask | SubstructureNotifyMask);
    XSync(display, False);

    // 4. Grab the Left Super key
    // This ensures we receive KeyPress/KeyRelease events for this key,
    // no matter which window has focus.
    KeyCode home_keycode = XKeysymToKeycode(display, XK_Super_L);
    XGrabKey(display, home_keycode, AnyModifier, root, True, GrabModeAsync, GrabModeAsync);

    // 5. Start the initial application (Window 0)
    start_initial_app(argv[1]);

    // 6. Main event loop
    XEvent ev;
    bool super_key_down = false; // State for handling key hold vs. press

    while (1)
    {
        XNextEvent(display, &ev); // Wait for the next event

        switch (ev.type)
        {
        case MapRequest:
        {
            // A window wants to be displayed
            XWindowAttributes wa;
            XGetWindowAttributes(display, ev.xmaprequest.window, &wa);

            // If window_zero is not set yet, this new window is our initial one
            if (window_zero == 0)
            {
                window_zero = ev.xmaprequest.window;
                printf("Window 0 has been identified (ID: %lu)\n", window_zero);
            }

            // Move and resize the window to be fullscreen
            XMoveResizeWindow(display, ev.xmaprequest.window, 0, 0, screen_width, screen_height);

            // Make the window visible
            XMapWindow(display, ev.xmaprequest.window);

            // Raise the window to the top of the stack, making it visible "in front"
            XRaiseWindow(display, ev.xmaprequest.window);
            
            // Set input focus to the new window so the user can type in it immediately
            XSetInputFocus(display, ev.xmaprequest.window, RevertToParent, CurrentTime);
            break;
        }
        case KeyPress:
        {
            // Check if the pressed key is our grabbed Super_L key
            if (ev.xkey.keycode == home_keycode)
            {
                // Only trigger the action on the initial press, not on auto-repeats while held down.
                if (!super_key_down)
                {
                    super_key_down = true; // Mark the key as being held down
                    printf("Super_L key pressed. Returning to home application.\n");
                    close_all_other_windows();

                    // After closing other windows, ensure the home window is raised and focused.
                    if (window_zero != 0)
                    {
                        XRaiseWindow(display, window_zero);
                        XSetInputFocus(display, window_zero, RevertToParent, CurrentTime);
                    }
                }
            }
            break;
        }
        case KeyRelease:
        {
            // Check if the released key is our grabbed Super_L key
            if (ev.xkey.keycode == home_keycode)
            {
                // Reset our state when the key is released.
                super_key_down = false;
            }
            break;
        }
        case DestroyNotify:
        {
            // A window was destroyed. If it was our Window 0, we should reset and restart it.
            // This prevents a new window from accidentally becoming the new unclosable Window 0.
            if (ev.xdestroywindow.window == window_zero)
            {
                printf("Window 0 has been closed. Restarting it.\n");
                window_zero = 0;           // Reset so the next mapped window becomes the new Window 0
                start_initial_app(argv[1]); // Relaunch the app
            }
            break;
        }
        case ConfigureRequest:
        {
            // Some applications may try to configure themselves (e.g., resize). We deny this
            // to maintain a strict fullscreen kiosk environment.
            XWindowChanges changes;
            changes.x = 0;
            changes.y = 0;
            changes.width = screen_width;
            changes.height = screen_height;
            XConfigureWindow(display, ev.xconfigurerequest.window, ev.xconfigurerequest.value_mask, &changes);
            break;
        }
        }
    }

    XCloseDisplay(display);
    return 0;
}