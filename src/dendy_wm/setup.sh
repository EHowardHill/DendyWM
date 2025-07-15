#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h> // For bool, true, false

// Global variables
static Display *display;
static Window root;
static Window window_zero = 0; // Will hold the ID of the initial window

// Custom X11 error handler
static int handle_x_error(Display *dpy, XErrorEvent *e) {
    char error_text[1024];
    XGetErrorText(dpy, e->error_code, error_text, sizeof(error_text));
    fprintf(stderr, "X Error: \n");
    fprintf(stderr, "    Request: %d, Error Code: %d (%s)\n", e->request_code, e->error_code, error_text);
    fprintf(stderr, "    Resource ID: %lu\n", e->resourceid);
    return 0; // Return 0 to indicate we've handled the error
}


// Function to start the initial application
void start_initial_app(const char *app_path)
{
    if (fork() == 0)
    {
        // We are in the child process
        // Close the display connection for the child process
        if(display) XCloseDisplay(display); 
        
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
        fprintf(stderr, "Failed to query window tree.\n");
        return;
    }

    printf("Closing all windows except Window 0 (ID: %lu)...\n", window_zero);

    for (unsigned int i = 0; i < num_children; i++)
    {
        // Crucially, check if the window is NOT window_zero
        if (children[i] != window_zero)
        {
            printf("Attempting to close window ID: %lu\n", children[i]);
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
    
    // Set our custom error handler
    XSetErrorHandler(handle_x_error);
    
    root = DefaultRootWindow(display);
    int screen_width = DisplayWidth(display, DefaultScreen(display));
    int screen_height = DisplayHeight(display, DefaultScreen(display));
    printf("WM: Display opened. Screen size: %dx%d\n", screen_width, screen_height);

    // 3. Become the Window Manager
    XSelectInput(display, root, SubstructureRedirectMask | SubstructureNotifyMask);
    XSync(display, False);
    printf("WM: Set as window manager for root window.\n");

    // 4. Grab the Left Super key
    KeyCode home_keycode = XKeysymToKeycode(display, XK_Super_L);
    if (XGrabKey(display, home_keycode, AnyModifier, root, True, GrabModeAsync, GrabModeAsync) == 0) {
        fprintf(stderr, "WM: ERROR! Failed to grab Super_L key. Another program may be using it.\n");
    } else {
        printf("WM: Successfully grabbed Super_L key.\n");
    }


    // 5. Start the initial application (Window 0)
    printf("WM: Starting initial application: %s\n", argv[1]);
    start_initial_app(argv[1]);

    // 6. Main event loop
    XEvent ev;
    bool super_key_down = false; 

    printf("WM: Entering main event loop...\n");
    while (1)
    {
        fflush(stdout); // Flush logs to ensure they are written immediately
        fflush(stderr);
        XNextEvent(display, &ev); 
        printf("WM: Received event type %d\n", ev.type);

        switch (ev.type)
        {
        case MapRequest:
        {
            printf("WM: MapRequest for window %lu\n", ev.xmaprequest.window);
            if (window_zero == 0)
            {
                window_zero = ev.xmaprequest.window;
                printf("WM: Window 0 has been identified (ID: %lu)\n", window_zero);
            }
            XMoveResizeWindow(display, ev.xmaprequest.window, 0, 0, screen_width, screen_height);
            XMapWindow(display, ev.xmaprequest.window);
            XRaiseWindow(display, ev.xmaprequest.window);
            XSetInputFocus(display, ev.xmaprequest.window, RevertToParent, CurrentTime);
            break;
        }
        case KeyPress:
        {
            if (ev.xkey.keycode == home_keycode)
            {
                if (!super_key_down)
                {
                    super_key_down = true; 
                    printf("WM: Super_L key pressed. Returning to home application.\n");
                    close_all_other_windows();
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
            if (ev.xkey.keycode == home_keycode)
            {
                super_key_down = false;
                printf("WM: Super_L key released.\n");
            }
            break;
        }
        case DestroyNotify:
        {
            printf("WM: DestroyNotify for window %lu\n", ev.xdestroywindow.window);
            if (ev.xdestroywindow.window == window_zero)
            {
                printf("WM: Window 0 has been closed. Restarting it.\n");
                window_zero = 0;
                start_initial_app(argv[1]);
            }
            break;
        }
        case ConfigureRequest:
        {
            printf("WM: ConfigureRequest for window %lu. Enforcing fullscreen.\n", ev.xconfigurerequest.window);
            XWindowChanges changes;
            changes.x = 0;
            changes.y = 0;
            changes.width = screen_width;
            changes.height = screen_height;
            XConfigureWindow(display, ev.xconfigurerequest.window, ev.xconfigurerequest.value_mask, &changes);
            break;
        }
        default:
            // Optional: Log unhandled events for debugging
            // printf("WM: Unhandled event type %d\n", ev.type);
            break;
        }
    }

    XCloseDisplay(display);
    return 0;
}
