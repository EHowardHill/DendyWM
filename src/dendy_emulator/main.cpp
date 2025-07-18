#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <stdexcept>
#include <dlfcn.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "/usr/include/libretro-common/libretro.h"

// SDL/OpenGL
SDL_Window *g_window = nullptr;
SDL_GLContext g_gl_context = nullptr;
SDL_AudioDeviceID g_audio_device = 0;

// Libretro function pointers (prefixed with 'core_' to avoid naming collisions)
void (*core_retro_init)(void);
void (*core_retro_deinit)(void);
unsigned (*core_retro_api_version)(void);
void (*core_retro_get_system_info)(struct retro_system_info *info);
void (*core_retro_get_system_av_info)(struct retro_system_av_info *info);
void (*core_retro_set_controller_port_device)(unsigned port, unsigned device);
void (*core_retro_reset)(void);
void (*core_retro_run)(void);
bool (*core_retro_load_game)(const struct retro_game_info *game);
void (*core_retro_unload_game)(void);
void *(*core_retro_get_memory_data)(unsigned id);
size_t (*core_retro_get_memory_size)(unsigned id);
void (*core_retro_set_video_refresh)(retro_video_refresh_t);
void (*core_retro_set_audio_sample)(retro_audio_sample_t);
void (*core_retro_set_audio_sample_batch)(retro_audio_sample_batch_t);
void (*core_retro_set_input_poll)(retro_input_poll_t);
void (*core_retro_set_input_state)(retro_input_state_t);
void (*core_retro_set_environment)(retro_environment_t);

// Application state
bool g_running = true;
void *g_core_handle = nullptr;
int16_t g_keyboard_state[16] = {0}; // State for all possible retro pad buttons for keyboard
int16_t g_joy_state[16] = {0};      // State for joystick
SDL_Joystick *g_joystick = nullptr;

// --- Core Mappings ---
// Maps file extensions to the name of the libretro core shared library file.
// On Debian, these are typically in /usr/lib/x86_64-linux-gnu/libretro/
const std::unordered_map<std::string, std::string> core_map = {
    {".gba", "mgba_libretro.so"},
    {".gbc", "mgba_libretro.so"},
    {".gb", "mgba_libretro.so"},
    {".sfc", "snes9x_libretro.so"},
    {".smc", "snes9x_libretro.so"},
    {".nes", "nestopia_libretro.so"},
    {".md", "genesis_plus_gx_libretro.so"},
    {".gen", "genesis_plus_gx_libretro.so"},
    {".gg", "genesis_plus_gx_libretro.so"},
    {".pce", "mednafen_pce_fast_libretro.so"},
};

// --- Libretro Callback Implementations ---

bool callback_environment(unsigned cmd, void *data)
{
    switch (cmd)
    {
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
    {
        // auto *cb = (struct retro_log_callback *)data; // Unused
        // We can redirect core logs to our own console here if desired.
        // For now, we do nothing.
        break;
    }
    case RETRO_ENVIRONMENT_GET_CAN_DUPE:
    {
        *(bool *)data = true;
        break;
    }
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
    {
        // auto format = *(enum retro_pixel_format *)data; // Unused
        // We only support hardware rendering with OpenGL for this simple frontend.
        // The core will ideally choose RGB565 or XRGB8888.
        // A more complex frontend would handle different pixel formats.
        return true; // Return true to indicate we handled this
    }
    default:
        // Log that we received an unhandled command
        return false;
    }
    return true;
}

void callback_video_refresh(const void *data, unsigned /*width*/, unsigned /*height*/, size_t /*pitch*/)
{
    // This callback is called once per frame from retro_run().
    // If 'data' is RETRO_HW_FRAME_BUFFER_VALID, the core has rendered directly to our
    // bound OpenGL context. We just need to swap the window buffers.
    // If 'data' contains pixel data, we would need to upload it to a texture and
    // render it to a quad. For this example, we assume hardware rendering.
    if (data == RETRO_HW_FRAME_BUFFER_VALID)
    {
        // Core rendered to hardware, nothing to do here.
    }
    else
    {
        // Software rendering path (not implemented for this simple example)
        // You would use glTexSubImage2D or similar here.
    }
}

void callback_audio_sample(int16_t left, int16_t right)
{
    int16_t buf[2] = {left, right};
    SDL_QueueAudio(g_audio_device, buf, sizeof(buf));
}

size_t callback_audio_sample_batch(const int16_t *data, size_t frames)
{
    SDL_QueueAudio(g_audio_device, data, frames * 2 * sizeof(int16_t));
    return frames; // Return the number of frames consumed
}

void callback_input_poll(void)
{
    // Clear previous state
    for (int i = 0; i < 16; ++i)
    {
        g_keyboard_state[i] = 0;
        g_joy_state[i] = 0;
    }

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        if (event.type == SDL_QUIT || (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE))
        {
            g_running = false;
        }
    }

    // --- Keyboard Input ---
    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    if (keys[SDL_SCANCODE_UP])
        g_keyboard_state[RETRO_DEVICE_ID_JOYPAD_UP] = 1;
    if (keys[SDL_SCANCODE_DOWN])
        g_keyboard_state[RETRO_DEVICE_ID_JOYPAD_DOWN] = 1;
    if (keys[SDL_SCANCODE_LEFT])
        g_keyboard_state[RETRO_DEVICE_ID_JOYPAD_LEFT] = 1;
    if (keys[SDL_SCANCODE_RIGHT])
        g_keyboard_state[RETRO_DEVICE_ID_JOYPAD_RIGHT] = 1;
    if (keys[SDL_SCANCODE_Z])
        g_keyboard_state[RETRO_DEVICE_ID_JOYPAD_A] = 1; // "A" button
    if (keys[SDL_SCANCODE_X])
        g_keyboard_state[RETRO_DEVICE_ID_JOYPAD_B] = 1; // "B" button
    if (keys[SDL_SCANCODE_A])
        g_keyboard_state[RETRO_DEVICE_ID_JOYPAD_X] = 1; // "X" button
    if (keys[SDL_SCANCODE_S])
        g_keyboard_state[RETRO_DEVICE_ID_JOYPAD_Y] = 1; // "Y" button
    if (keys[SDL_SCANCODE_RETURN])
        g_keyboard_state[RETRO_DEVICE_ID_JOYPAD_START] = 1;
    if (keys[SDL_SCANCODE_RSHIFT])
        g_keyboard_state[RETRO_DEVICE_ID_JOYPAD_SELECT] = 1;
    if (keys[SDL_SCANCODE_Q])
        g_keyboard_state[RETRO_DEVICE_ID_JOYPAD_L] = 1;
    if (keys[SDL_SCANCODE_W])
        g_keyboard_state[RETRO_DEVICE_ID_JOYPAD_R] = 1;

    // --- Joystick Input ---
    if (g_joystick)
    {
        SDL_JoystickUpdate();
        // This mapping is a guess for a typical Gamecube-style controller.
        // It may need adjustment for your specific controller.
        if (SDL_JoystickGetButton(g_joystick, 0))
            g_joy_state[RETRO_DEVICE_ID_JOYPAD_A] = 1; // A button
        if (SDL_JoystickGetButton(g_joystick, 1))
            g_joy_state[RETRO_DEVICE_ID_JOYPAD_B] = 1; // B button
        if (SDL_JoystickGetButton(g_joystick, 2))
            g_joy_state[RETRO_DEVICE_ID_JOYPAD_X] = 1; // X button
        if (SDL_JoystickGetButton(g_joystick, 3))
            g_joy_state[RETRO_DEVICE_ID_JOYPAD_Y] = 1; // Y button
        if (SDL_JoystickGetButton(g_joystick, 4))
            g_joy_state[RETRO_DEVICE_ID_JOYPAD_L] = 1; // L shoulder
        if (SDL_JoystickGetButton(g_joystick, 5))
            g_joy_state[RETRO_DEVICE_ID_JOYPAD_R] = 1; // R shoulder
        if (SDL_JoystickGetButton(g_joystick, 9))
            g_joy_state[RETRO_DEVICE_ID_JOYPAD_START] = 1; // Start button
        // Select button is often missing on GC controllers, map to Z trigger?
        if (SDL_JoystickGetButton(g_joystick, 6))
            g_joy_state[RETRO_DEVICE_ID_JOYPAD_SELECT] = 1; // Z trigger

        // D-Pad (often handled as a hat)
        Uint8 hat = SDL_JoystickGetHat(g_joystick, 0);
        if (hat & SDL_HAT_UP)
            g_joy_state[RETRO_DEVICE_ID_JOYPAD_UP] = 1;
        if (hat & SDL_HAT_DOWN)
            g_joy_state[RETRO_DEVICE_ID_JOYPAD_DOWN] = 1;
        if (hat & SDL_HAT_LEFT)
            g_joy_state[RETRO_DEVICE_ID_JOYPAD_LEFT] = 1;
        if (hat & SDL_HAT_RIGHT)
            g_joy_state[RETRO_DEVICE_ID_JOYPAD_RIGHT] = 1;
    }
}

int16_t callback_input_state(unsigned port, unsigned /*device*/, unsigned /*index*/, unsigned id)
{
    if (port == 0 && id < 16)
    {
        // If joystick is active, it takes priority. Otherwise, use keyboard.
        if (g_joystick)
        {
            return g_joy_state[id];
        }
        return g_keyboard_state[id];
    }
    return 0; // Only support one player
}

// --- Helper Functions ---

#define LOAD_SYM(V, S)                                              \
    do                                                              \
    {                                                               \
        V = (decltype(V))dlsym(g_core_handle, #S);                  \
        if (!V)                                                     \
            throw std::runtime_error("Failed to load symbol: " #S); \
    } while (0)

void load_core(const std::string &core_path)
{
    g_core_handle = dlopen(core_path.c_str(), RTLD_LAZY);
    if (!g_core_handle)
    {
        throw std::runtime_error("Failed to load core: " + core_path + " - " + dlerror());
    }

    // Load all the required libretro functions from the .so file
    LOAD_SYM(core_retro_init, retro_init);
    LOAD_SYM(core_retro_deinit, retro_deinit);
    LOAD_SYM(core_retro_api_version, retro_api_version);
    LOAD_SYM(core_retro_get_system_info, retro_get_system_info);
    LOAD_SYM(core_retro_get_system_av_info, retro_get_system_av_info);
    LOAD_SYM(core_retro_set_controller_port_device, retro_set_controller_port_device);
    LOAD_SYM(core_retro_reset, retro_reset);
    LOAD_SYM(core_retro_run, retro_run);
    LOAD_SYM(core_retro_load_game, retro_load_game);
    LOAD_SYM(core_retro_unload_game, retro_unload_game);
    LOAD_SYM(core_retro_get_memory_data, retro_get_memory_data);
    LOAD_SYM(core_retro_get_memory_size, retro_get_memory_size);
    LOAD_SYM(core_retro_set_video_refresh, retro_set_video_refresh);
    LOAD_SYM(core_retro_set_audio_sample, retro_set_audio_sample);
    LOAD_SYM(core_retro_set_audio_sample_batch, retro_set_audio_sample_batch);
    LOAD_SYM(core_retro_set_input_poll, retro_set_input_poll);
    LOAD_SYM(core_retro_set_input_state, retro_set_input_state);
    LOAD_SYM(core_retro_set_environment, retro_set_environment);

    // Set the callbacks
    core_retro_set_environment(callback_environment);
    core_retro_set_video_refresh(callback_video_refresh);
    core_retro_set_audio_sample(callback_audio_sample);
    core_retro_set_audio_sample_batch(callback_audio_sample_batch);
    core_retro_set_input_poll(callback_input_poll);
    core_retro_set_input_state(callback_input_state);
}

void init_sdl_gl()
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK) != 0)
    {
        throw std::runtime_error("Failed to initialize SDL: " + std::string(SDL_GetError()));
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    g_window = SDL_CreateWindow("Retro Launcher", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_OPENGL);
    if (!g_window)
    {
        throw std::runtime_error("Failed to create SDL window: " + std::string(SDL_GetError()));
    }

    g_gl_context = SDL_GL_CreateContext(g_window);
    if (!g_gl_context)
    {
        throw std::runtime_error("Failed to create OpenGL context: " + std::string(SDL_GetError()));
    }
    SDL_GL_MakeCurrent(g_window, g_gl_context);
    SDL_GL_SetSwapInterval(1); // Enable VSync

    // Initialize Joystick
    if (SDL_NumJoysticks() > 0)
    {
        g_joystick = SDL_JoystickOpen(0);
        if (g_joystick)
        {
            std::cout << "Joystick detected: " << SDL_JoystickName(g_joystick) << std::endl;
        }
    }
    else
    {
        std::cout << "No joystick detected. Using keyboard only." << std::endl;
    }
}

void init_audio(double sample_rate)
{
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = static_cast<int>(sample_rate);
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = NULL; // We will queue audio

    g_audio_device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (g_audio_device == 0)
    {
        throw std::runtime_error("Failed to open audio device: " + std::string(SDL_GetError()));
    }
    SDL_PauseAudioDevice(g_audio_device, 0); // Start playing
}

bool load_rom(const std::string &rom_path)
{
    // The core will handle loading from the path, but some cores require the data
    // to be loaded into memory first. We will do both.
    std::vector<uint8_t> rom_data;
    struct retro_game_info game_info = {};
    game_info.path = rom_path.c_str();

    std::ifstream file(rom_path, std::ios::binary | std::ios::ate);
    if (file)
    {
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        rom_data.resize(size);
        if (file.read((char *)rom_data.data(), size))
        {
            game_info.data = rom_data.data();
            game_info.size = rom_data.size();
        }
    }
    else
    {
        std::cerr << "Failed to open ROM file: " << rom_path << std::endl;
        return false;
    }

    if (!core_retro_load_game(&game_info))
    {
        std::cerr << "The core failed to load the game." << std::endl;
        return false;
    }
    return true;
}

void cleanup()
{
    if (core_retro_unload_game)
        core_retro_unload_game();
    if (core_retro_deinit)
        core_retro_deinit();
    if (g_core_handle)
        dlclose(g_core_handle);

    if (g_joystick)
        SDL_JoystickClose(g_joystick);
    if (g_audio_device > 0)
        SDL_CloseAudioDevice(g_audio_device);
    if (g_gl_context)
        SDL_GL_DeleteContext(g_gl_context);
    if (g_window)
        SDL_DestroyWindow(g_window);
    SDL_Quit();
}

// --- Main Application ---

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <path-to-rom>" << std::endl;
        return 1;
    }

    std::string rom_path = argv[1];
    std::string core_path;

    try
    {
        // Determine core from ROM extension
        std::string extension = std::filesystem::path(rom_path).extension().string();
        auto it = core_map.find(extension);
        if (it == core_map.end())
        {
            throw std::runtime_error("Unsupported ROM extension: " + extension);
        }
        // On Debian, cores are in a standard path
        core_path = "/usr/lib/x86_64-linux-gnu/libretro/" + it->second;

        std::cout << "ROM: " << rom_path << std::endl;
        std::cout << "Core: " << core_path << std::endl;

        // Initialization
        init_sdl_gl();
        load_core(core_path);

        core_retro_init();

        // Get timing info from core and initialize audio
        struct retro_system_av_info av_info;
        core_retro_get_system_av_info(&av_info);
        init_audio(av_info.timing.sample_rate);

        // Window size might be based on core geometry
        int w_width, w_height;
        SDL_GetWindowSize(g_window, &w_width, &w_height);
        glViewport(0, 0, w_width, w_height);

        if (!load_rom(rom_path))
        {
            throw std::runtime_error("Failed to load ROM.");
        }

        // Main loop
        while (g_running)
        {
            // Poll input inside loop as well to catch quit events
            callback_input_poll();

            core_retro_run();
            SDL_GL_SwapWindow(g_window);
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "FATAL ERROR: " << e.what() << std::endl;
        cleanup();
        return 1;
    }

    // Cleanup
    std::cout << "Exiting..." << std::endl;
    cleanup();
    return 0;
}