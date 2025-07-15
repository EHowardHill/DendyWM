#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <iostream>
#include <stdexcept>
#include <vector>      // Required for tracking multiple windows
#include <algorithm>   // Required for std::find
#include <cstdlib>
#include <unistd.h>    // for fork, execvp, setsid
#include <csignal>     // for signal

// A global flag to detect if another window manager is already running.
// The X11 error handler will set this to true if it catches a specific error.
static bool another_wm_running = false;

// Custom X11 error handler.
// Xlib is not thread-safe and uses this global function approach.
// It will be called if a request to the X server fails. We are specifically
// looking for a BadAccess error when we try to become the window manager,
// which indicates another one is already active.
static int x_error_handler(Display* dpy, XErrorEvent* ee) {
    if (ee->error_code == BadAccess) {
        another_wm_running = true;
    }
    // We don't print other errors to keep the output clean, but in a real
    // application, you would want to log them.
    return 0; // The return value is ignored.
}

class WindowManager {
public:
    // Constructor: Initializes the connection to the X server.
    WindowManager(const char* app_path) : app_path_(app_path) {
        // 1. Open a connection to the X server.
        // `nullptr` means use the DISPLAY environment variable.
        display_ = XOpenDisplay(nullptr);
        if (!display_) {
            throw std::runtime_error("Failed to open X display.");
        }

        // 2. Get basic screen information.
        screen_ = DefaultScreen(display_);
        root_ = RootWindow(display_, screen_);
        screen_width_ = DisplayWidth(display_, screen_);
        screen_height_ = DisplayHeight(display_, screen_);
        
        // The client_windows_ vector is automatically initialized to be empty.

        std::cout << "Screen dimensions: " << screen_width_ << "x" << screen_height_ << std::endl;
    }

    // Destructor: Cleans up the connection.
    ~WindowManager() {
        if (display_) {
            XCloseDisplay(display_);
        }
    }

    // The main entry point to run the window manager.
    void run() {
        // 1. Set our custom error handler to detect if another WM is running.
        XSetErrorHandler(x_error_handler);

        // 2. Select events we want to listen for on the root window.
        // SubstructureRedirectMask is the key: it tells the X server to send
        // us events for mapping/configuring top-level windows instead of
        // doing it automatically. This is how a window manager takes control.
        XSelectInput(display_, root_, SubstructureRedirectMask | SubstructureNotifyMask);

        // 3. Synchronize with the X server. This flushes the request buffer
        // and ensures our error handler is called if the XSelectInput request
        // failed (e.g., because another WM is running).
        XSync(display_, False);

        if (another_wm_running) {
            throw std::runtime_error("Another window manager is already running.");
        }
        std::cout << "Successfully became the window manager." << std::endl;

        // 4. Launch the initial application.
        launch_initial_app();

        // 5. Enter the main event loop.
        event_loop();
    }

private:
    // Forks and executes the initial application.
    void launch_initial_app() {
        pid_t pid = fork();
        if (pid < 0) {
            throw std::runtime_error("Failed to fork.");
        }

        if (pid == 0) { // Child process
            // Detach from the controlling terminal
            setsid();

            // Prepare arguments for execvp. It needs a null-terminated array.
            char* const args[] = {const_cast<char*>(app_path_), nullptr};
            execvp(app_path_, args);

            // If execvp returns, an error occurred.
            perror("execvp failed");
            _exit(127); // Use _exit in child after fork
        }
        std::cout << "Launched application: " << app_path_ << " with PID " << pid << std::endl;
    }

    // The main loop that listens for and handles X events.
    void event_loop() {
        XEvent ev;
        for (;;) {
            // Get the next event from the queue. This call blocks.
            XNextEvent(display_, &ev);

            switch (ev.type) {
                // An application wants to be displayed (mapped) on the screen.
                case MapRequest:
                    handle_map_request(ev.xmaprequest);
                    break;

                // An application wants to change its size or position.
                case ConfigureRequest:
                    handle_configure_request(ev.xconfigurerequest);
                    break;

                // A window was closed/destroyed.
                case DestroyNotify:
                    handle_window_closed(ev.xdestroywindow.window);
                    break;
                case UnmapNotify:
                    // We handle UnmapNotify as well because some applications unmap their
                    // window before destroying it. This handler is idempotent.
                    handle_window_closed(ev.xunmap.window);
                    break;
            }
        }
    }

    // Handles a MapRequest event. This is where new windows are managed.
    void handle_map_request(const XMapRequestEvent& e) {
        std::cout << "Handling MapRequest for window " << e.window << std::endl;

        // --- CHANGED ---
        // Instead of ignoring new windows, we manage all of them.

        // 1. Force the new window to be fullscreen.
        XMoveResizeWindow(display_, e.window, 0, 0, screen_width_, screen_height_);

        // 2. Make the window visible.
        XMapWindow(display_, e.window);
        
        // 3. Raise the window to the top of the stacking order.
        XRaiseWindow(display_, e.window);
        
        // 4. Give the window input focus.
        XSetInputFocus(display_, e.window, RevertToParent, CurrentTime);

        // 5. Add the window to our list of managed clients.
        client_windows_.push_back(e.window);
    }

    // Handles a ConfigureRequest event.
    void handle_configure_request(const XConfigureRequestEvent& e) {
        // The application is trying to resize or move itself. We deny this
        // by re-asserting our fullscreen configuration to maintain the kiosk mode.
        XWindowChanges changes;
        changes.x = 0;
        changes.y = 0;
        changes.width = screen_width_;
        changes.height = screen_height_;
        changes.border_width = 0;
        changes.sibling = e.above;
        changes.stack_mode = e.detail;

        // Apply our configuration instead of the one the application requested.
        XConfigureWindow(display_, e.window, e.value_mask, &changes);
        std::cout << "Handled ConfigureRequest, forcing fullscreen." << std::endl;
    }
    
    // --- NEW ---
    // Handles a window being closed (unmapped or destroyed).
    void handle_window_closed(Window w) {
        // Find the closed window in our list.
        auto it = std::find(client_windows_.begin(), client_windows_.end(), w);

        if (it != client_windows_.end()) {
            std::cout << "Client window " << w << " was closed." << std::endl;
            
            // Remove the window from our list.
            client_windows_.erase(it);

            // Check if any windows are left.
            if (client_windows_.empty()) {
                std::cout << "Last client window closed. Exiting." << std::endl;
                XCloseDisplay(display_);
                display_ = nullptr; // Prevent double-close in destructor
                exit(0);
            } else {
                // If other windows remain, give focus to the new top-most one
                // (which is now the last element in our vector).
                Window top_window = client_windows_.back();
                XSetInputFocus(display_, top_window, RevertToParent, CurrentTime);
                 std::cout << "Gave focus to window " << top_window << std::endl;
            }
        }
    }

    Display* display_;
    int screen_;
    Window root_;
    int screen_width_;
    int screen_height_;
    const char* app_path_;
    // --- CHANGED ---
    // We now use a vector to keep track of all managed windows in stacking order.
    // The last element is the top-most window.
    std::vector<Window> client_windows_;
};

// Main function
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <path_to_application>" << std::endl;
        std::cerr << "Example: " << argv[0] << " /usr/bin/xterm" << std::endl;
        return 1;
    }

    try {
        WindowManager wm(argv[1]);
        wm.run();
    } catch (const std::runtime_error& e) {
        std::cerr << "Fatal Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
