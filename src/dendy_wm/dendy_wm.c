#define WLR_USE_UNSTABLE
#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>

struct KioskCompositor;

struct KioskWindow
{
    struct wl_list link;
    struct wlr_xdg_toplevel *xdg_toplevel;
    struct wlr_scene_tree *scene_tree;
    struct wl_listener map;
    struct wl_listener unmap;
    struct wl_listener destroy;
    struct KioskCompositor *compositor;
    bool mapped;
    pid_t pid;
};

struct KioskOutput
{
    struct wl_list link;
    struct wlr_output *wlr_output;
    struct wl_listener frame;
    struct wl_listener destroy;
    struct KioskCompositor *compositor;
};

struct KioskKeyboard
{
    struct wl_list link;
    struct wlr_keyboard *wlr_keyboard;
    struct wl_listener key;
    struct wl_listener destroy;
    struct wl_listener modifiers;
    struct KioskCompositor *compositor;
    struct timespec enter_press_time;
    bool enter_held;
};

struct KioskCompositor
{
    struct wl_display *wl_display;
    struct wlr_backend *backend;
    struct wlr_renderer *renderer;
    struct wlr_allocator *allocator;
    struct wlr_scene *scene;
    struct wlr_scene_output_layout *scene_layout;
    struct wlr_output_layout *output_layout;

    struct wlr_xdg_shell *xdg_shell;
    struct wl_listener new_xdg_surface;
    struct wl_listener new_output;

    struct wlr_seat *seat;
    struct wl_listener new_input;
    struct wl_listener request_cursor;

    struct wl_list windows;
    struct wl_list outputs;
    struct wl_list keyboards;

    pid_t home_pid;
    bool first_window;
};

// -----------------------------------------------------------------------------
// Output Callbacks and Functions
// -----------------------------------------------------------------------------

static void output_frame(struct wl_listener *listener, void *data)
{
    struct KioskOutput *output = wl_container_of(listener, output, frame);
    struct wlr_scene *scene = output->compositor->scene;

    struct wlr_scene_output *scene_output = wlr_scene_get_scene_output(
        scene, output->wlr_output);

    wlr_scene_output_commit(scene_output, NULL);

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_scene_output_send_frame_done(scene_output, &now);
}

static void output_destroy(struct wl_listener *listener, void *data)
{
    struct KioskOutput *output = wl_container_of(listener, output, destroy);

    wl_list_remove(&output->frame.link);
    wl_list_remove(&output->destroy.link);
    wl_list_remove(&output->link);
    free(output);
}

static void new_output(struct wl_listener *listener, void *data)
{
    struct KioskCompositor *compositor = wl_container_of(listener, compositor, new_output);
    struct wlr_output *wlr_output = (struct wlr_output *)data;

    wlr_output_init_render(wlr_output, compositor->allocator, compositor->renderer);

    struct wlr_output_state state;
    wlr_output_state_init(&state);
    wlr_output_state_set_enabled(&state, true);

    struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
    if (mode != NULL)
    {
        wlr_output_state_set_mode(&state, mode);
    }

    wlr_output_commit_state(wlr_output, &state);
    wlr_output_state_finish(&state);

    struct KioskOutput *output = calloc(1, sizeof(struct KioskOutput));
    if (!output)
    {
        wlr_log(WLR_ERROR, "Cannot allocate KioskOutput");
        return;
    }
    output->wlr_output = wlr_output;
    output->compositor = compositor;

    output->frame.notify = output_frame;
    wl_signal_add(&wlr_output->events.frame, &output->frame);

    output->destroy.notify = output_destroy;
    wl_signal_add(&wlr_output->events.destroy, &output->destroy);

    wlr_output_layout_add_auto(compositor->output_layout, wlr_output);
    wlr_scene_output_create(compositor->scene, wlr_output);

    wl_list_insert(&compositor->outputs, &output->link);
}

// -----------------------------------------------------------------------------
// Window (XDG Toplevel) Callbacks and Functions
// -----------------------------------------------------------------------------

static void window_map(struct wl_listener *listener, void *data)
{
    struct KioskWindow *window = wl_container_of(listener, window, map);
    window->mapped = true;

    // Bring to front
    wlr_scene_node_raise_to_top(&window->scene_tree->node);

    // Focus the new window
    struct wlr_surface *surface = window->xdg_toplevel->base->surface;
    struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(window->compositor->seat);
    if (keyboard != NULL)
    {
        wlr_seat_keyboard_notify_enter(window->compositor->seat, surface,
                                       keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
    }
}

static void window_unmap(struct wl_listener *listener, void *data)
{
    struct KioskWindow *window = wl_container_of(listener, window, unmap);
    window->mapped = false;
}

static void window_destroy(struct wl_listener *listener, void *data)
{
    struct KioskWindow *window = wl_container_of(listener, window, destroy);

    wl_list_remove(&window->map.link);
    wl_list_remove(&window->unmap.link);
    wl_list_remove(&window->destroy.link);
    wl_list_remove(&window->link);
    free(window);
}

static void new_xdg_surface(struct wl_listener *listener, void *data)
{
    struct KioskCompositor *compositor = wl_container_of(listener, compositor, new_xdg_surface);
    struct wlr_xdg_surface *xdg_surface = (struct wlr_xdg_surface *)data;

    if (xdg_surface->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL)
    {
        return;
    }

    struct KioskWindow *window = calloc(1, sizeof(struct KioskWindow));
    if (!window)
    {
        wlr_log(WLR_ERROR, "Cannot allocate KioskWindow");
        return;
    }
    window->xdg_toplevel = xdg_surface->toplevel;
    window->compositor = compositor;
    window->mapped = false;
    window->pid = 0;

    // Create scene tree for this window
    window->scene_tree = wlr_scene_xdg_surface_create(
        &compositor->scene->tree, xdg_surface);
    window->scene_tree->node.data = window;

    // If this is the first window, it's our home window
    if (compositor->first_window)
    {
        window->pid = compositor->home_pid;
        compositor->first_window = false;
    }

    window->map.notify = window_map;
    wl_signal_add(&xdg_surface->surface->events.map, &window->map);

    window->unmap.notify = window_unmap;
    wl_signal_add(&xdg_surface->surface->events.unmap, &window->unmap);

    window->destroy.notify = window_destroy;
    wl_signal_add(&xdg_surface->events.destroy, &window->destroy);

    // Configure to fullscreen
    if (!wl_list_empty(&compositor->outputs))
    {
        wlr_xdg_toplevel_set_fullscreen(xdg_surface->toplevel, true);
        wlr_xdg_toplevel_set_wm_capabilities(xdg_surface->toplevel,
                                             WLR_XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN);
    }

    wl_list_insert(&compositor->windows, &window->link);
}

// -----------------------------------------------------------------------------
// Keyboard Callbacks and Functions
// -----------------------------------------------------------------------------

static void keyboard_handle_modifiers(struct wl_listener *listener, void *data)
{
    struct KioskKeyboard *keyboard = wl_container_of(listener, keyboard, modifiers);
    wlr_seat_set_keyboard(keyboard->compositor->seat, keyboard->wlr_keyboard);
    wlr_seat_keyboard_notify_modifiers(keyboard->compositor->seat,
                                       &keyboard->wlr_keyboard->modifiers);
}

static void keyboard_handle_key(struct wl_listener *listener, void *data)
{
    struct KioskKeyboard *keyboard = wl_container_of(listener, keyboard, key);
    struct KioskCompositor *compositor = keyboard->compositor;
    struct wlr_keyboard_key_event *event = (struct wlr_keyboard_key_event *)data;

    uint32_t keycode = event->keycode + 8;
    const xkb_keysym_t *syms;
    int nsyms = xkb_state_key_get_syms(keyboard->wlr_keyboard->xkb_state, keycode, &syms);

    bool handled = false;

    for (int i = 0; i < nsyms; i++)
    {
        if (syms[i] == XKB_KEY_Return || syms[i] == XKB_KEY_KP_Enter)
        {
            if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED)
            {
                if (!keyboard->enter_held)
                {
                    keyboard->enter_held = true;
                    clock_gettime(CLOCK_MONOTONIC, &keyboard->enter_press_time);
                }

                struct timespec now;
                clock_gettime(CLOCK_MONOTONIC, &now);
                long seconds = now.tv_sec - keyboard->enter_press_time.tv_sec;

                if (seconds >= 2)
                {
                    // Close all windows except the home window
                    struct KioskWindow *window, *tmp;
                    wl_list_for_each_safe(window, tmp, &compositor->windows, link)
                    {
                        if (window->pid != compositor->home_pid)
                        {
                            wlr_xdg_toplevel_send_close(window->xdg_toplevel);
                        }
                    }
                    handled = true;
                }
            }
            else
            { // Released
                keyboard->enter_held = false;
            }
        }
    }

    if (!handled)
    {
        wlr_seat_set_keyboard(compositor->seat, keyboard->wlr_keyboard);
        wlr_seat_keyboard_notify_key(compositor->seat, event->time_msec,
                                     event->keycode, event->state);
    }
}

static void keyboard_handle_destroy(struct wl_listener *listener, void *data)
{
    struct KioskKeyboard *keyboard = wl_container_of(listener, keyboard, destroy);

    wl_list_remove(&keyboard->modifiers.link);
    wl_list_remove(&keyboard->key.link);
    wl_list_remove(&keyboard->destroy.link);
    wl_list_remove(&keyboard->link);
    free(keyboard);
}

// -----------------------------------------------------------------------------
// Input and Seat Callbacks
// -----------------------------------------------------------------------------

static void new_input(struct wl_listener *listener, void *data)
{
    struct KioskCompositor *compositor = wl_container_of(listener, compositor, new_input);
    struct wlr_input_device *device = (struct wlr_input_device *)data;

    if (device->type == WLR_INPUT_DEVICE_KEYBOARD)
    {
        struct wlr_keyboard *wlr_keyboard = wlr_keyboard_from_input_device(device);

        struct KioskKeyboard *keyboard = calloc(1, sizeof(struct KioskKeyboard));
        if (!keyboard)
        {
            wlr_log(WLR_ERROR, "Cannot allocate KioskKeyboard");
            return;
        }
        keyboard->wlr_keyboard = wlr_keyboard;
        keyboard->compositor = compositor;
        keyboard->enter_held = false;

        struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
        struct xkb_keymap *keymap = xkb_keymap_new_from_names(context, NULL,
                                                              XKB_KEYMAP_COMPILE_NO_FLAGS);

        wlr_keyboard_set_keymap(wlr_keyboard, keymap);
        xkb_keymap_unref(keymap);
        xkb_context_unref(context);

        wlr_keyboard_set_repeat_info(wlr_keyboard, 25, 600);

        keyboard->modifiers.notify = keyboard_handle_modifiers;
        wl_signal_add(&wlr_keyboard->events.modifiers, &keyboard->modifiers);

        keyboard->key.notify = keyboard_handle_key;
        wl_signal_add(&wlr_keyboard->events.key, &keyboard->key);

        keyboard->destroy.notify = keyboard_handle_destroy;
        wl_signal_add(&device->events.destroy, &keyboard->destroy);

        wlr_seat_set_keyboard(compositor->seat, wlr_keyboard);

        wl_list_insert(&compositor->keyboards, &keyboard->link);
    }
}

static void seat_request_cursor(struct wl_listener *listener, void *data)
{
    // Hide cursor in kiosk mode - don't process cursor requests
}

// -----------------------------------------------------------------------------
// Main Entrypoint
// -----------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    wlr_log_init(WLR_DEBUG, NULL);

    char home_application[256] = {0};

    // If an argument is passed, use it as the home application path
    if (argc > 1)
    {
        wlr_log(WLR_INFO, "Using command-line argument for home app: %s", argv[1]);
        strncpy(home_application, argv[1], sizeof(home_application) - 1);
    }
    // Otherwise, read the path from the config file
    else
    {
        FILE *fptr = fopen("/etc/dendy/home_application", "r");
        if (fptr == NULL)
        {
            wlr_log(WLR_ERROR, "Could not open 'home_application' config file.");
            return 1;
        }

        if (fgets(home_application, sizeof(home_application), fptr) == NULL)
        {
            wlr_log(WLR_ERROR, "'home_application' file is empty or cannot be read.");
            fclose(fptr);
            return 1;
        }
        fclose(fptr);

        // Remove the trailing newline character from the file input
        home_application[strcspn(home_application, "\n")] = 0;
    }

    struct KioskCompositor compositor = {0};
    compositor.first_window = true;
    wl_list_init(&compositor.windows);
    wl_list_init(&compositor.outputs);
    wl_list_init(&compositor.keyboards);

    compositor.wl_display = wl_display_create();

    compositor.backend = wlr_backend_autocreate(compositor.wl_display, NULL);
    if (!compositor.backend)
    {
        wlr_log(WLR_ERROR, "Failed to create backend");
        return 1;
    }

    compositor.renderer = wlr_renderer_autocreate(compositor.backend);
    if (!compositor.renderer)
    {
        wlr_log(WLR_ERROR, "Failed to create renderer");
        return 1;
    }

    wlr_renderer_init_wl_display(compositor.renderer, compositor.wl_display);

    compositor.allocator = wlr_allocator_autocreate(compositor.backend,
                                                    compositor.renderer);
    if (!compositor.allocator)
    {
        wlr_log(WLR_ERROR, "Failed to create allocator");
        return 1;
    }

    wlr_compositor_create(compositor.wl_display, 5, compositor.renderer);
    wlr_data_device_manager_create(compositor.wl_display);

    compositor.output_layout = wlr_output_layout_create();

    compositor.scene = wlr_scene_create();
    compositor.scene_layout = wlr_scene_attach_output_layout(compositor.scene,
                                                             compositor.output_layout);

    compositor.xdg_shell = wlr_xdg_shell_create(compositor.wl_display, 3);
    compositor.new_xdg_surface.notify = new_xdg_surface;
    wl_signal_add(&compositor.xdg_shell->events.new_surface,
                  &compositor.new_xdg_surface);

    compositor.new_output.notify = new_output;
    wl_signal_add(&compositor.backend->events.new_output, &compositor.new_output);

    compositor.seat = wlr_seat_create(compositor.wl_display, "seat0");
    compositor.request_cursor.notify = seat_request_cursor;
    wl_signal_add(&compositor.seat->events.request_set_cursor,
                  &compositor.request_cursor);

    compositor.new_input.notify = new_input;
    wl_signal_add(&compositor.backend->events.new_input, &compositor.new_input);

    const char *socket = wl_display_add_socket_auto(compositor.wl_display);
    if (!socket)
    {
        wlr_log(WLR_ERROR, "Failed to add socket");
        return 1;
    }

    if (!wlr_backend_start(compositor.backend))
    {
        wlr_log(WLR_ERROR, "Failed to start backend");
        return 1;
    }

    setenv("WAYLAND_DISPLAY", socket, true);
    wlr_log(WLR_INFO, "Running on WAYLAND_DISPLAY=%s", socket);

    // Launch the home application
    compositor.home_pid = fork();
    if (compositor.home_pid == 0)
    {
        sleep(1); // Give compositor time to start
        execl(home_application, home_application, NULL);
        perror("execl failed");
        _exit(1);
    }
    else if (compositor.home_pid < 0)
    {
        wlr_log(WLR_ERROR, "Failed to fork home process");
        return 1;
    }

    wl_display_run(compositor.wl_display);

    // Resources are freed by their respective destroy listeners.
    wl_display_destroy_clients(compositor.wl_display);
    wl_display_destroy(compositor.wl_display);

    return 0;
}