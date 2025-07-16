#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <unistd.h>
#include <csignal>
#include <chrono>

static bool another_wm_running = false;

static int x_error_handler(Display *dpy, XErrorEvent *ee)
{
    if (ee->error_code == BadAccess)
    {
        another_wm_running = true;
    }
    return 0;
}

class WindowManager
{
public:
    WindowManager(const char *app_path) : app_path_(app_path),
                                          super_key_pressed_(false),
                                          initial_window_(None)
    {
        display_ = XOpenDisplay(nullptr);
        if (!display_)
        {
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
    ~WindowManager()
    {
        if (display_)
        {
            XCloseDisplay(display_);
        }
    }

    // The main entry point to run the window manager.
    void run()
    {
        XSetErrorHandler(x_error_handler);

        // Need to select events before becoming WM to avoid race conditions
        XSelectInput(display_, root_, SubstructureRedirectMask | SubstructureNotifyMask | KeyPressMask | KeyReleaseMask);
        XSync(display_, False);

        if (another_wm_running)
        {
            throw std::runtime_error("Another window manager is already running.");
        }
        std::cout << "Successfully became the window manager." << std::endl;

        // Grab the Super key
        grab_super_key();

        launch_initial_app();

        // Allow some time for the initial app to create its window
        usleep(100000); // 100ms

        event_loop();
    }

private:
    // Grabs the Super key for global hotkey functionality
    void grab_super_key()
    {
        KeyCode super_keycode = XKeysymToKeycode(display_, XK_Super_L);
        if (super_keycode == 0)
        {
            std::cerr << "Warning: Could not find Super_L key" << std::endl;
            return;
        }

        // Grab the key with any modifier combination
        XGrabKey(display_, super_keycode, AnyModifier, root_, True, GrabModeAsync, GrabModeAsync);
        std::cout << "Grabbed Super_L key (keycode: " << (int)super_keycode << ")" << std::endl;
    }

    // Forks and executes the initial application.
    void launch_initial_app()
    {
        pid_t pid = fork();
        if (pid < 0)
        {
            throw std::runtime_error("Failed to fork.");
        }

        if (pid == 0)
        { // Child process
            // Detach from the controlling terminal
            setsid();

            // Prepare arguments for execvp. It needs a null-terminated array.
            char *const args[] = {const_cast<char *>(app_path_), nullptr};
            execvp(app_path_, args);

            // If execvp returns, an error occurred.
            perror("execvp failed");
            _exit(127); // Use _exit in child after fork
        }
        std::cout << "Launched application: " << app_path_ << " with PID " << pid << std::endl;
    }

    // The main loop that listens for and handles X events.
    void event_loop()
    {
        XEvent ev;
        for (;;)
        {
            // Check if we need to process the Super key timeout
            if (super_key_pressed_)
            {
                // Use select with timeout to check for events
                fd_set fds;
                FD_ZERO(&fds);
                int x11_fd = ConnectionNumber(display_);
                FD_SET(x11_fd, &fds);

                struct timeval tv;
                tv.tv_sec = 0;
                tv.tv_usec = 50000; // 50ms timeout

                if (select(x11_fd + 1, &fds, nullptr, nullptr, &tv) == 0)
                {
                    // Timeout occurred, check if 3 seconds have passed
                    auto now = std::chrono::steady_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - super_press_start_);

                    if (duration.count() >= 2000)
                    {
                        std::cout << "Super key held for 2 seconds, closing all windows except initial" << std::endl;
                        close_all_except_initial();
                        super_key_pressed_ = false;
                    }
                    continue;
                }
            }

            // Get the next event from the queue. This call blocks if no timeout is set.
            XNextEvent(display_, &ev);

            switch (ev.type)
            {
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
                handle_window_destroyed(ev.xdestroywindow.window);
                break;
            case UnmapNotify:
                // Only handle unmap if we didn't cause it
                if (ev.xunmap.send_event == False)
                {
                    handle_window_unmapped(ev.xunmap.window);
                }
                break;

            // Key events
            case KeyPress:
                handle_key_press(ev.xkey);
                break;
            case KeyRelease:
                handle_key_release(ev.xkey);
                break;
            }
        }
    }

    // Handles key press events
    void handle_key_press(const XKeyEvent &e)
    {
        KeySym keysym = XLookupKeysym(const_cast<XKeyEvent *>(&e), 0);

        if (keysym == XK_Super_L)
        {
            if (!super_key_pressed_)
            {
                super_key_pressed_ = true;
                super_press_start_ = std::chrono::steady_clock::now();
                std::cout << "Super_L key pressed, starting timer" << std::endl;
            }
        }
    }

    // Handles key release events
    void handle_key_release(const XKeyEvent &e)
    {
        KeySym keysym = XLookupKeysym(const_cast<XKeyEvent *>(&e), 0);

        if (keysym == XK_Super_L)
        {
            if (super_key_pressed_)
            {
                super_key_pressed_ = false;
                auto now = std::chrono::steady_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - super_press_start_);
                std::cout << "Super_L key released after " << duration.count() << "ms" << std::endl;
            }
        }
    }

    // Closes all windows except the initial one
    void close_all_except_initial()
    {
        if (initial_window_ == None || client_windows_.empty())
        {
            std::cout << "No windows to close or initial window not set" << std::endl;
            return;
        }

        // Create a copy of the windows list to iterate safely
        std::vector<Window> windows_to_close;
        for (Window w : client_windows_)
        {
            if (w != initial_window_)
            {
                windows_to_close.push_back(w);
            }
        }

        std::cout << "Closing " << windows_to_close.size() << " windows (keeping window " << initial_window_ << ")" << std::endl;

        // Close each window
        for (Window w : windows_to_close)
        {
            std::cout << "Closing window " << w << std::endl;

            // Try to close the window gracefully first
            XEvent close_event;
            close_event.type = ClientMessage;
            close_event.xclient.window = w;
            close_event.xclient.message_type = XInternAtom(display_, "WM_PROTOCOLS", False);
            close_event.xclient.format = 32;
            close_event.xclient.data.l[0] = XInternAtom(display_, "WM_DELETE_WINDOW", False);
            close_event.xclient.data.l[1] = CurrentTime;

            XSendEvent(display_, w, False, NoEventMask, &close_event);

            // Also forcefully destroy the window
            XDestroyWindow(display_, w);
        }

        XFlush(display_);

        // Ensure the initial window is on top and focused
        if (initial_window_ != None)
        {
            XRaiseWindow(display_, initial_window_);
            XSetInputFocus(display_, initial_window_, RevertToParent, CurrentTime);
            std::cout << "Raised and focused initial window " << initial_window_ << std::endl;
        }
    }

    // Handles a MapRequest event. This is where new windows are managed.
    void handle_map_request(const XMapRequestEvent &e)
    {
        std::cout << "Handling MapRequest for window " << e.window << std::endl;

        // Set the initial window if not set yet (first window from launched app)
        if (initial_window_ == None && client_windows_.empty())
        {
            initial_window_ = e.window;
            std::cout << "Set initial window to " << initial_window_ << std::endl;
        }

        // Configure the window to fullscreen
        XMoveResizeWindow(display_, e.window, 0, 0, screen_width_, screen_height_);

        // Map the window first
        XMapWindow(display_, e.window);

        // Add to our window list
        client_windows_.push_back(e.window);

        // Ensure the new window is on top and has focus
        XRaiseWindow(display_, e.window);
        XSetInputFocus(display_, e.window, RevertToParent, CurrentTime);

        // Force a sync to ensure the window is properly displayed
        XSync(display_, False);

        std::cout << "Mapped window " << e.window << " (total windows: " << client_windows_.size() << ")" << std::endl;
    }

    // Handles a ConfigureRequest event.
    void handle_configure_request(const XConfigureRequestEvent &e)
    {
        XWindowChanges changes;
        changes.x = 0;
        changes.y = 0;
        changes.width = screen_width_;
        changes.height = screen_height_;
        changes.border_width = 0;
        changes.sibling = e.above;
        changes.stack_mode = e.detail;

        // Only configure size/position, don't change stacking unless explicitly requested
        unsigned long value_mask = e.value_mask & ~(CWSibling | CWStackMode);
        if (e.value_mask & (CWSibling | CWStackMode))
        {
            // If stacking is requested, honor it
            value_mask = e.value_mask;
        }

        XConfigureWindow(display_, e.window, value_mask, &changes);
        std::cout << "Handled ConfigureRequest for window " << e.window << std::endl;
    }

    void handle_window_destroyed(Window w)
    {
        auto it = std::find(client_windows_.begin(), client_windows_.end(), w);

        if (it != client_windows_.end())
        {
            std::cout << "Client window " << w << " was destroyed." << std::endl;

            // Remove the window from our list.
            client_windows_.erase(it);

            // Check if any windows are left.
            if (client_windows_.empty())
            {
                std::cout << "Last client window closed. Exiting." << std::endl;
                XCloseDisplay(display_);
                display_ = nullptr; // Prevent double-close in destructor
                exit(0);
            }
            else
            {
                // Focus the topmost remaining window
                Window top_window = client_windows_.back();
                XSetInputFocus(display_, top_window, RevertToParent, CurrentTime);
                XRaiseWindow(display_, top_window);
                std::cout << "Gave focus to window " << top_window << std::endl;
            }
        }
    }

    void handle_window_unmapped(Window w)
    {
        // For now, treat unmapping the same as destroying
        // Some applications might unmap windows temporarily
        std::cout << "Window " << w << " was unmapped" << std::endl;
        handle_window_destroyed(w);
    }

    Display *display_;
    int screen_;
    Window root_;
    int screen_width_;
    int screen_height_;
    const char *app_path_;
    std::vector<Window> client_windows_;
    Window initial_window_;
    bool super_key_pressed_;
    std::chrono::steady_clock::time_point super_press_start_;
};

// Main function
int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <path_to_application>" << std::endl;
        std::cerr << "Example: " << argv[0] << " /usr/bin/xterm" << std::endl;
        return 1;
    }

    try
    {
        WindowManager wm(argv[1]);
        wm.run();
    }
    catch (const std::runtime_error &e)
    {
        std::cerr << "Fatal Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}