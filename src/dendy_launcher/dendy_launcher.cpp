// dendy_launcher.cpp
#include "include/raylib.h"
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cmath>
#include <map>
#include <memory>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <sys/wait.h>

namespace fs = std::filesystem;

// Configuration structure to hold all settings
struct LauncherConfig
{
    // Window settings
    int initial_window_width = 1920;
    int initial_window_height = 1080;

    // Grid settings
    int min_grid_cols = 3;
    int max_grid_cols = 5;
    int cell_width = 300;
    int cell_height = 300;

    // Icon settings
    int icon_size = 196;
    int icon_padding = 64;

    // Layout settings
    int top_margin = 128;
    int bottom_margin = 100;
    int scroll_padding = 50;
    int text_height = 32;

    // Animation settings
    float scroll_speed = 15.0f;
    float smooth_scroll_factor = 0.15f;
    float gamepad_deadzone = 0.25f;
    float selection_scale = 1.1f;
    float animation_speed = 0.2f;
    float fade_in_duration = 0.1f;
    float tile_stagger_delay = 0.03f;
    float tile_animation_duration = 0.5f;
    float launch_animation_duration = 0.6f;

    // Asset paths
    std::string bg_music = "assets/bg02.mp3";
    std::string snd_move = "assets/move.wav";
    std::string snd_select = "assets/select.wav";
    std::string snd_login = "assets/login.wav";
    std::string font_url = "assets/fonts/Bogart-Black-trial.ttf";
    std::string font_url_launcher = "assets/fonts/Bogart-Medium-trial.ttf";
    std::string logo_url = "assets/logo.png";

    // Configuration file path
    static constexpr const char *CONFIG_FILE = "config/launcher.ini";
};

// Configuration parser class
class ConfigParser
{
private:
    static std::string trim(const std::string &str)
    {
        size_t first = str.find_first_not_of(" \t\n\r");
        if (first == std::string::npos)
            return "";
        size_t last = str.find_last_not_of(" \t\n\r");
        return str.substr(first, (last - first + 1));
    }

    static std::pair<std::string, std::string> parseKeyValue(const std::string &line)
    {
        size_t pos = line.find('=');
        if (pos == std::string::npos)
            return {"", ""};

        std::string key = trim(line.substr(0, pos));
        std::string value = trim(line.substr(pos + 1));

        // Remove quotes if present
        if (value.length() >= 2 && value.front() == '"' && value.back() == '"')
        {
            value = value.substr(1, value.length() - 2);
        }

        return {key, value};
    }

public:
    static LauncherConfig LoadConfig(const std::string &filepath)
    {
        LauncherConfig config;

        // Try to open the config file
        std::ifstream file(filepath);
        if (!file.is_open())
        {
            std::cout << "Config file not found at: " << filepath << ". Using default values." << std::endl;

            // Create a default config file
            SaveDefaultConfig(filepath, config);
            return config;
        }

        std::string line;
        std::string currentSection;

        while (std::getline(file, line))
        {
            line = trim(line);

            // Skip empty lines and comments
            if (line.empty() || line[0] == '#' || line[0] == ';')
                continue;

            // Check for section header
            if (line[0] == '[' && line.back() == ']')
            {
                currentSection = line.substr(1, line.length() - 2);
                continue;
            }

            // Parse key-value pair
            auto [key, value] = parseKeyValue(line);
            if (key.empty())
                continue;

            // Apply settings based on section
            if (currentSection == "Window")
            {
                if (key == "initial_width")
                    config.initial_window_width = std::stoi(value);
                else if (key == "initial_height")
                    config.initial_window_height = std::stoi(value);
            }
            else if (currentSection == "Grid")
            {
                if (key == "min_cols")
                    config.min_grid_cols = std::stoi(value);
                else if (key == "max_cols")
                    config.max_grid_cols = std::stoi(value);
                else if (key == "cell_width")
                    config.cell_width = std::stoi(value);
                else if (key == "cell_height")
                    config.cell_height = std::stoi(value);
            }
            else if (currentSection == "Icons")
            {
                if (key == "icon_size")
                    config.icon_size = std::stoi(value);
                else if (key == "icon_padding")
                    config.icon_padding = std::stoi(value);
            }
            else if (currentSection == "Layout")
            {
                if (key == "top_margin")
                    config.top_margin = std::stoi(value);
                else if (key == "bottom_margin")
                    config.bottom_margin = std::stoi(value);
                else if (key == "scroll_padding")
                    config.scroll_padding = std::stoi(value);
                else if (key == "text_height")
                    config.text_height = std::stoi(value);
            }
            else if (currentSection == "Animation")
            {
                if (key == "scroll_speed")
                    config.scroll_speed = std::stof(value);
                else if (key == "smooth_scroll_factor")
                    config.smooth_scroll_factor = std::stof(value);
                else if (key == "gamepad_deadzone")
                    config.gamepad_deadzone = std::stof(value);
                else if (key == "selection_scale")
                    config.selection_scale = std::stof(value);
                else if (key == "animation_speed")
                    config.animation_speed = std::stof(value);
                else if (key == "fade_in_duration")
                    config.fade_in_duration = std::stof(value);
                else if (key == "tile_stagger_delay")
                    config.tile_stagger_delay = std::stof(value);
                else if (key == "tile_animation_duration")
                    config.tile_animation_duration = std::stof(value);
                else if (key == "launch_animation_duration")
                    config.launch_animation_duration = std::stof(value);
            }
            else if (currentSection == "Assets")
            {
                if (key == "bg_music")
                    config.bg_music = value;
                else if (key == "snd_move")
                    config.snd_move = value;
                else if (key == "snd_select")
                    config.snd_select = value;
                else if (key == "snd_login")
                    config.snd_login = value;
                else if (key == "font_url")
                    config.font_url = value;
                else if (key == "font_url_launcher")
                    config.font_url_launcher = value;
                else if (key == "logo_url")
                    config.logo_url = value;
            }
        }

        file.close();
        std::cout << "Configuration loaded from: " << filepath << std::endl;
        return config;
    }

    static void SaveDefaultConfig(const std::string &filepath, const LauncherConfig &config)
    {
        // Create directory if it doesn't exist
        fs::path path(filepath);
        fs::create_directories(path.parent_path());

        std::ofstream file(filepath);
        if (!file.is_open())
        {
            std::cerr << "Failed to create default config file: " << filepath << std::endl;
            return;
        }

        file << "# Dendy Launcher Configuration File\n";
        file << "# Modify these values to customize the launcher appearance and behavior\n\n";

        file << "[Window]\n";
        file << "initial_width = " << config.initial_window_width << "\n";
        file << "initial_height = " << config.initial_window_height << "\n\n";

        file << "[Grid]\n";
        file << "min_cols = " << config.min_grid_cols << "\n";
        file << "max_cols = " << config.max_grid_cols << "\n";
        file << "cell_width = " << config.cell_width << "\n";
        file << "cell_height = " << config.cell_height << "\n\n";

        file << "[Icons]\n";
        file << "icon_size = " << config.icon_size << "\n";
        file << "icon_padding = " << config.icon_padding << "\n\n";

        file << "[Layout]\n";
        file << "top_margin = " << config.top_margin << "\n";
        file << "bottom_margin = " << config.bottom_margin << "\n";
        file << "scroll_padding = " << config.scroll_padding << "\n";
        file << "text_height = " << config.text_height << "\n\n";

        file << "[Animation]\n";
        file << "scroll_speed = " << config.scroll_speed << "\n";
        file << "smooth_scroll_factor = " << config.smooth_scroll_factor << "\n";
        file << "gamepad_deadzone = " << config.gamepad_deadzone << "\n";
        file << "selection_scale = " << config.selection_scale << "\n";
        file << "animation_speed = " << config.animation_speed << "\n";
        file << "fade_in_duration = " << config.fade_in_duration << "\n";
        file << "tile_stagger_delay = " << config.tile_stagger_delay << "\n";
        file << "tile_animation_duration = " << config.tile_animation_duration << "\n";
        file << "launch_animation_duration = " << config.launch_animation_duration << "\n\n";

        file << "[Assets]\n";
        file << "bg_music = \"" << config.bg_music << "\"\n";
        file << "snd_move = \"" << config.snd_move << "\"\n";
        file << "snd_select = \"" << config.snd_select << "\"\n";
        file << "snd_login = \"" << config.snd_login << "\"\n";
        file << "font_url = \"" << config.font_url << "\"\n";
        file << "font_url_launcher = \"" << config.font_url_launcher << "\"\n";
        file << "logo_url = \"" << config.logo_url << "\"\n";

        file.close();
        std::cout << "Default configuration file created at: " << filepath << std::endl;
    }
};

enum AnimationState
{
    ANIM_FADE_IN,
    ANIM_NORMAL,
    ANIM_LAUNCHING
};

bool hasBeginning(std::string const &fullString, std::string const &beginning)
{
    if (fullString.length() >= beginning.length())
    {
        return (0 == fullString.compare(0, beginning.length(), beginning));
    }
    else
    {
        return false;
    }
}

bool hasEnding(std::string const &fullString, std::string const &ending)
{
    if (fullString.length() >= ending.length())
    {
        return (0 == fullString.compare(fullString.length() - ending.length(), ending.length(), ending));
    }
    else
    {
        return false;
    }
}

class AppEntry
{
public:
    std::string name;
    std::string exec;
    std::string icon;
    Texture2D texture;
    bool hasTexture;
    float scale;
    float targetScale;

    // Animation properties
    float animDelay;
    float animProgress;
    Vector2 animOffset;
    float opacity;

    AppEntry() : hasTexture(false), scale(1.0f), targetScale(1.0f),
                 animDelay(0.0f), animProgress(0.0f), animOffset({0, 0}), opacity(0.0f) {}

    ~AppEntry()
    {
        if (hasTexture)
        {
            UnloadTexture(texture);
        }
    }

    // Disable copy to avoid texture issues
    AppEntry(const AppEntry &) = delete;
    AppEntry &operator=(const AppEntry &) = delete;

    // Enable move
    AppEntry(AppEntry &&other) noexcept
        : name(std::move(other.name)),
          exec(std::move(other.exec)),
          icon(std::move(other.icon)),
          texture(other.texture),
          hasTexture(other.hasTexture),
          scale(other.scale),
          targetScale(other.targetScale),
          animDelay(other.animDelay),
          animProgress(other.animProgress),
          animOffset(other.animOffset),
          opacity(other.opacity)
    {
        other.hasTexture = false;
    }

    AppEntry &operator=(AppEntry &&other) noexcept
    {
        if (this != &other)
        {
            if (hasTexture)
                UnloadTexture(texture);
            name = std::move(other.name);
            exec = std::move(other.exec);
            icon = std::move(other.icon);
            texture = other.texture;
            hasTexture = other.hasTexture;
            scale = other.scale;
            targetScale = other.targetScale;
            animDelay = other.animDelay;
            animProgress = other.animProgress;
            animOffset = other.animOffset;
            opacity = other.opacity;
            other.hasTexture = false;
        }
        return *this;
    }

    void UpdateAnimation(float animationSpeed)
    {
        scale += (targetScale - scale) * animationSpeed;
    }

    void UpdateFadeInAnimation(float deltaTime, float tileAnimationDuration)
    {
        if (animProgress < 1.0f)
        {
            animProgress = std::min(1.0f, animProgress + deltaTime / tileAnimationDuration);

            // Easing function for smooth animation
            float easedProgress = 1.0f - pow(1.0f - animProgress, 3.0f);

            // Fade in opacity
            opacity = easedProgress;

            // Slide up animation
            animOffset.y = (1.0f - easedProgress) * 30.0f;

            // Scale animation
            scale = 0.8f + 0.2f * easedProgress;
        }
    }

    void UpdateLaunchAnimation(float progress, int index, Vector2 centerPoint)
    {
        // Calculate direction from center
        Vector2 direction = {
            animOffset.x - centerPoint.x,
            animOffset.y - centerPoint.y};

        // Normalize and apply force
        float length = sqrt(direction.x * direction.x + direction.y * direction.y);
        if (length > 0)
        {
            direction.x /= length;
            direction.y /= length;
        }
        else
        {
            // Random direction if at center
            direction.x = cos(index * 0.5f);
            direction.y = sin(index * 0.5f);
        }

        // Accelerating motion
        float force = progress * progress * 1000.0f;
        animOffset.x += direction.x * force;
        animOffset.y += direction.y * force;

        // Fade out
        opacity = 1.0f - progress;

        // Spin and shrink
        scale = (1.0f - progress * 0.5f) * targetScale;
    }
};

class DesktopFileParser
{
private:
    static std::string Trim(const std::string &str)
    {
        size_t first = str.find_first_not_of(" \t\n\r");
        if (first == std::string::npos)
            return "";
        size_t last = str.find_last_not_of(" \t\n\r");
        return str.substr(first, (last - first + 1));
    }

public:
    static std::unique_ptr<AppEntry> ParseFile(const fs::path &filepath)
    {
        std::ifstream file(filepath);
        if (!file.is_open())
            return nullptr;

        auto app = std::make_unique<AppEntry>();
        std::string line;
        bool inDesktopEntry = false;
        bool isValid = true;

        while (std::getline(file, line))
        {
            line = Trim(line);

            if (line == "[Desktop Entry]")
            {
                inDesktopEntry = true;
            }
            else if (hasBeginning(line, "["))
            {
                inDesktopEntry = false;
            }
            else if (inDesktopEntry && !line.empty())
            {
                if (hasBeginning(line, "Name=") && app->name.empty())
                {
                    app->name = line.substr(5);
                }
                else if (hasBeginning(line, "Exec="))
                {
                    app->exec = line.substr(5);
                    // Remove field codes like %f, %F, %u, %U
                    size_t pos = app->exec.find(" %");
                    if (pos != std::string::npos)
                    {
                        app->exec = app->exec.substr(0, pos);
                    }
                }
                else if (hasBeginning(line, "Icon="))
                {
                    app->icon = line.substr(5);
                }
                else if (line == "NoDisplay=true" || line == "Hidden=true")
                {
                    isValid = false;
                    break;
                }
            }
        }

        if (!isValid || app->name.empty() || app->exec.empty())
        {
            return nullptr;
        }

        return app;
    }
};

class IconLoader
{
private:
    static std::vector<std::string> GetIconSearchPaths()
    {
        std::vector<std::string> paths = {
            "/usr/share/icons/hicolor",
            "/usr/share/icons/gnome",
            "/usr/share/icons/Adwaita",
            "/usr/share/pixmaps"};

        // Add Waydroid-specific paths
        std::string home = getenv("HOME") ? getenv("HOME") : "";
        if (!home.empty())
        {
            // Waydroid typically stores icons in these locations
            paths.push_back(home + "/.local/share/icons/hicolor");
            paths.push_back(home + "/.local/share/icons");
            paths.push_back(home + "/.local/share/pixmaps");
            paths.push_back(home + "/.local/share/waydroid-extra/icons");

            // Check for Waydroid data directory
            std::string waydroidData = home + "/.local/share/waydroid/data";
            if (fs::exists(waydroidData))
            {
                paths.push_back(waydroidData + "/icons");
            }
        }

        // Also check system-wide Waydroid locations
        paths.push_back("/var/lib/waydroid/images/icons");
        paths.push_back("/usr/share/waydroid-extra/icons");

        return paths;
    }

    static std::vector<std::string> GetIconSizes()
    {
        // Add more sizes that Android apps might use
        return {"128x128", "256x256", "192x192", "144x144", "96x96", "72x72", "scalable", "64x64", "48x48"};
    }

    // Helper function to check if this is a Waydroid app
    static bool IsWaydroidApp(const std::string &iconName)
    {
        return iconName.find("waydroid") != std::string::npos ||
               iconName.find("android") != std::string::npos ||
               iconName.find("org.") == 0 || // Android package names often start with org.
               iconName.find("com.") == 0;   // or com.
    }

public:
    static std::string FindIcon(const std::string &iconName)
    {
        // Debug output
        std::cout << "Looking for icon: " << iconName << std::endl;

        // If it's already a full path
        if (iconName[0] == '/' && fs::exists(iconName))
        {
            std::cout << "Found icon at full path: " << iconName << std::endl;
            return iconName;
        }

        // If it's a relative path starting with ~
        if (iconName[0] == '~')
        {
            std::string home = getenv("HOME") ? getenv("HOME") : "";
            if (!home.empty())
            {
                std::string expandedPath = home + iconName.substr(1);
                if (fs::exists(expandedPath))
                {
                    std::cout << "Found icon at expanded path: " << expandedPath << std::endl;
                    return expandedPath;
                }
            }
        }

        std::vector<std::string> extensions = {".png", ".jpg", ".jpeg", ".svg", ".xpm", ""};
        bool isWaydroid = IsWaydroidApp(iconName);

        for (const auto &basePath : GetIconSearchPaths())
        {
            // Skip if path doesn't exist
            if (!fs::exists(basePath))
                continue;

            // Direct pixmaps search
            if (basePath.find("pixmaps") != std::string::npos)
            {
                for (const auto &ext : extensions)
                {
                    std::string path = basePath + "/" + iconName + ext;
                    if (fs::exists(path))
                    {
                        std::cout << "Found icon at: " << path << std::endl;
                        return path;
                    }
                }

                // For Waydroid apps, also try without the full package name
                if (isWaydroid && iconName.find('.') != std::string::npos)
                {
                    std::string shortName = iconName.substr(iconName.rfind('.') + 1);
                    for (const auto &ext : extensions)
                    {
                        std::string path = basePath + "/" + shortName + ext;
                        if (fs::exists(path))
                        {
                            std::cout << "Found icon with short name at: " << path << std::endl;
                            return path;
                        }
                    }
                }
                continue;
            }

            // Themed icon search
            for (const auto &size : GetIconSizes())
            {
                // Try multiple subdirectories
                std::vector<std::string> subdirs = {"apps", "applications", ""};

                for (const auto &subdir : subdirs)
                {
                    std::string dirPath = basePath + "/" + size;
                    if (!subdir.empty())
                    {
                        dirPath += "/" + subdir;
                    }

                    for (const auto &ext : extensions)
                    {
                        std::string path = dirPath + "/" + iconName + ext;
                        if (fs::exists(path))
                        {
                            std::cout << "Found icon at: " << path << std::endl;
                            return path;
                        }
                    }
                }
            }
        }

        // Special handling for Waydroid apps - try to find Android APK icons
        if (isWaydroid)
        {
            std::string home = getenv("HOME") ? getenv("HOME") : "";
            if (!home.empty())
            {
                // Check Waydroid overlay directory
                std::string overlayPath = home + "/.local/share/waydroid/overlay";
                if (fs::exists(overlayPath))
                {
                    // This would need more complex logic to extract from APKs
                    std::cout << "Could check Waydroid overlay at: " << overlayPath << std::endl;
                }
            }
        }

        std::cout << "Icon not found for: " << iconName << std::endl;
        return "";
    }

    // Returns true if icon was loaded successfully, false otherwise
    static bool TryLoadIconTexture(const std::string &iconPath, Texture2D &tex, int iconSize)
    {
        if (iconPath.empty())
        {
            return false; // No icon path, skip this app
        }

        // Load image based on extension
        if (hasEnding(iconPath, ".svg"))
        {
            // SVG files not supported without additional library
            return false;
        }
        else
        {
            Image img = LoadImage(iconPath.c_str());
            if (img.data)
            {
                // Resize to standard icon size while maintaining aspect ratio
                float scale = std::min((float)iconSize / img.width, (float)iconSize / img.height);
                int newWidth = img.width * scale;
                int newHeight = img.height * scale;

                ImageResize(&img, newWidth, newHeight);

                // Create a new image with padding if needed
                if (newWidth < iconSize || newHeight < iconSize)
                {
                    Image paddedImg = GenImageColor(iconSize, iconSize, BLANK);
                    int offsetX = (iconSize - newWidth) / 2;
                    int offsetY = (iconSize - newHeight) / 2;
                    ImageDraw(&paddedImg, img,
                              (Rectangle){0, 0, (float)newWidth, (float)newHeight},
                              (Rectangle){(float)offsetX, (float)offsetY, (float)newWidth, (float)newHeight},
                              WHITE);
                    UnloadImage(img);
                    img = paddedImg;
                }

                tex = LoadTextureFromImage(img);
                UnloadImage(img);

                // Set texture filtering for better quality
                SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);

                return true; // Successfully loaded
            }
            else
            {
                return false; // Failed to load image
            }
        }
    }
};

class AppLauncher
{
private:
    LauncherConfig config;
    std::vector<std::unique_ptr<AppEntry>> apps;
    int selectedIndex;
    int hoveredIndex;
    float scrollY;
    float targetScrollY;
    float maxScrollY;
    Font font;
    int currentGridCols;
    int lastWindowWidth;
    int lastWindowHeight;
    bool wasFocusedLastFrame = true;
    Music music;
    Sound fxMove;
    Sound fxSelect;
    Font fontBold;
    Texture2D logoTexture;

    // Animation state
    AnimationState animState;
    float animTimer;
    float fadeAlpha;
    int launchingAppIndex;
    std::string pendingLaunchCommand;

    void LoadApplicationsFromDirectory(const fs::path &dir)
    {
        if (!fs::exists(dir) || !fs::is_directory(dir))
            return;

        for (const auto &entry : fs::directory_iterator(dir))
        {
            if (entry.path().extension() == ".desktop")
            {
                auto app = DesktopFileParser::ParseFile(entry.path());
                if (app)
                {
                    apps.push_back(std::move(app));
                }
            }
        }
    }

    void SortApplications()
    {
        std::sort(apps.begin(), apps.end(),
                  [](const std::unique_ptr<AppEntry> &a, const std::unique_ptr<AppEntry> &b)
                  {
                      return std::lexicographical_compare(
                          a->name.begin(), a->name.end(),
                          b->name.begin(), b->name.end(),
                          [](char c1, char c2)
                          { return std::tolower(c1) < std::tolower(c2); });
                  });
    }

    void LoadIcons()
    {
        std::cout << "Loading icons for " << apps.size() << " applications..." << std::endl;

        // Use erase-remove idiom to filter out apps without valid icons
        apps.erase(
            std::remove_if(apps.begin(), apps.end(),
                           [this](std::unique_ptr<AppEntry> &app)
                           {
                               std::string iconPath = IconLoader::FindIcon(app->icon);

                               if (iconPath.empty())
                               {
                                   std::cout << "No icon found for: " << app->name << " (icon: " << app->icon << ")" << std::endl;
                                   return true; // Remove this app
                               }

                               // Try to load the texture
                               bool success = IconLoader::TryLoadIconTexture(iconPath, app->texture, config.icon_size);
                               if (success)
                               {
                                   app->hasTexture = true;
                                   std::cout << "Successfully loaded icon for: " << app->name << std::endl;
                                   return false; // Keep this app
                               }
                               else
                               {
                                   std::cout << "Failed to load icon texture for: " << app->name << " (path: " << iconPath << ")" << std::endl;
                                   return true; // Remove this app
                               }
                           }),
            apps.end());

        std::cout << "After filtering, " << apps.size() << " applications have valid icons" << std::endl;
    }

    void InitializeAnimations()
    {
        // Set up staggered animation delays for Windows Phone effect
        for (int i = 0; i < (int)apps.size(); i++)
        {
            apps[i]->animDelay = i * config.tile_stagger_delay;
            apps[i]->animProgress = 0.0f;
            apps[i]->opacity = 0.0f;
        }
    }

    int CalculateGridColumns(int windowWidth) const
    {
        int cols = windowWidth / config.cell_width;
        return std::clamp(cols, config.min_grid_cols, config.max_grid_cols);
    }

    void UpdateMaxScroll()
    {
        int windowHeight = GetScreenHeight();
        int rows = ((int)apps.size() + currentGridCols - 1) / currentGridCols;
        float contentHeight = rows * config.cell_height + config.top_margin + config.bottom_margin;
        maxScrollY = std::max(0.0f, contentHeight - windowHeight);
    }

    Rectangle GetCellRect(int index) const
    {
        int windowWidth = GetScreenWidth();
        int row = index / currentGridCols;
        int col = index % currentGridCols;

        float gridWidth = currentGridCols * config.cell_width;
        float x = (windowWidth - gridWidth) / 2 + col * config.cell_width;
        float y = row * config.cell_height - scrollY + config.top_margin;

        return {x, y, (float)config.cell_width, (float)config.cell_height};
    }

    void LaunchApp(int index)
    {
        PauseMusicStream(music);
        PlaySound(fxSelect);

        if (index >= 0 && index < (int)apps.size())
        {
            // Start launch animation
            animState = ANIM_LAUNCHING;
            animTimer = 0.0f;
            launchingAppIndex = index;
            pendingLaunchCommand = apps[index]->exec + " &";

            // Set initial positions for launch animation
            for (int i = 0; i < (int)apps.size(); i++)
            {
                Rectangle rect = GetCellRect(i);
                apps[i]->animOffset.x = rect.x + rect.width / 2;
                apps[i]->animOffset.y = rect.y + rect.height / 2;
            }
        }
    }

    void CheckWindowResize()
    {
        int windowWidth = GetScreenWidth();
        int windowHeight = GetScreenHeight();

        if (windowWidth != lastWindowWidth || windowHeight != lastWindowHeight)
        {
            lastWindowWidth = windowWidth;
            lastWindowHeight = windowHeight;

            // Recalculate grid columns
            int newGridCols = CalculateGridColumns(windowWidth);

            // If grid columns changed, adjust selected index to stay on same app
            if (newGridCols != currentGridCols && selectedIndex >= 0)
            {
                int row = selectedIndex / currentGridCols;
                int col = selectedIndex % currentGridCols;

                // Clamp column to new grid
                col = std::min(col, newGridCols - 1);
                selectedIndex = row * newGridCols + col;

                // Ensure selected index is valid
                selectedIndex = std::min(selectedIndex, (int)apps.size() - 1);
            }

            currentGridCols = newGridCols;
            UpdateMaxScroll();

            // Ensure scroll is within bounds
            targetScrollY = std::clamp(targetScrollY, 0.0f, maxScrollY);
        }
    }

public:
    AppLauncher() : selectedIndex(0), hoveredIndex(-1), scrollY(0), targetScrollY(0), maxScrollY(0),
                    animState(ANIM_FADE_IN), animTimer(0.0f), fadeAlpha(1.0f),
                    launchingAppIndex(-1)
    {
        // Load configuration
        config = ConfigParser::LoadConfig(LauncherConfig::CONFIG_FILE);

        currentGridCols = CalculateGridColumns(config.initial_window_width);
        lastWindowWidth = config.initial_window_width;
        lastWindowHeight = config.initial_window_height;

        // Load fonts
        font = LoadFontEx(config.font_url_launcher.c_str(), 32, nullptr, 0);
        if (!font.texture.id)
        {
            font = GetFontDefault();
        }

        fontBold = LoadFontEx(config.font_url.c_str(), 96, 0, 250);
        if (!fontBold.texture.id)
        {
            fontBold = GetFontDefault();
        }

        // Load logo
        Image logoImage = LoadImage(config.logo_url.c_str());
        if (logoImage.data)
        {
            logoTexture = LoadTextureFromImage(logoImage);
            UnloadImage(logoImage);
        }

        // Load sounds
        music = LoadMusicStream(config.bg_music.c_str());
        fxMove = LoadSound(config.snd_move.c_str());
        fxSelect = LoadSound(config.snd_select.c_str());

        // Start music
        PlayMusicStream(music);
    }

    ~AppLauncher()
    {
        if (font.texture.id)
            UnloadFont(font);
        if (fontBold.texture.id)
            UnloadFont(fontBold);
        if (logoTexture.id)
            UnloadTexture(logoTexture);
        if (music.ctxData)
            UnloadMusicStream(music);
        if (fxMove.frameCount)
            UnloadSound(fxMove);
        if (fxSelect.frameCount)
            UnloadSound(fxSelect);
    }

    void LoadApplications()
    {
        apps.clear();

        // Load from standard directories
        LoadApplicationsFromDirectory("/usr/share/applications");
        LoadApplicationsFromDirectory("/usr/local/share/applications");

        // Load from user directory
        std::string home = getenv("HOME") ? getenv("HOME") : "";
        if (!home.empty())
        {
            LoadApplicationsFromDirectory(home + "/.local/share/applications");
        }

        SortApplications();
        LoadIcons(); // This now filters out apps without icons
        InitializeAnimations();
        UpdateMaxScroll();

        // Reset selected index if no apps remain
        if (apps.empty())
        {
            selectedIndex = -1;
        }
        else if (selectedIndex >= (int)apps.size())
        {
            selectedIndex = 0;
        }
    }

    void HandleInput()
    {
        // Don't handle input during animations or if no apps
        if (animState == ANIM_LAUNCHING || apps.empty())
            return;

        hoveredIndex = -1;

        // Check for window resize
        CheckWindowResize();

        // Mouse input
        Vector2 mousePos = GetMousePosition();
        for (int i = 0; i < (int)apps.size(); i++)
        {
            Rectangle cellRect = GetCellRect(i);
            if (CheckCollisionPointRec(mousePos, cellRect))
            {
                hoveredIndex = i;
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                {
                    LaunchApp(i);
                }
                break;
            }
        }

        // Keyboard navigation
        int cols = currentGridCols;
        if (IsKeyPressed(KEY_RIGHT) && selectedIndex % cols < cols - 1 && selectedIndex < (int)apps.size() - 1)
        {
            selectedIndex++;
            PlaySound(fxMove);
        }
        if (IsKeyPressed(KEY_LEFT) && selectedIndex % cols > 0)
        {
            selectedIndex--;
            PlaySound(fxMove);
        }
        if (IsKeyPressed(KEY_DOWN) && selectedIndex + cols < (int)apps.size())
        {
            selectedIndex += cols;
            PlaySound(fxMove);
        }
        if (IsKeyPressed(KEY_UP) && selectedIndex - cols >= 0)
        {
            selectedIndex -= cols;
            PlaySound(fxMove);
        }
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
        {
            LaunchApp(selectedIndex);
        }

        // Gamepad navigation
        if (IsGamepadAvailable(0))
        {
            float axisX = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X);
            float axisY = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y);

            static float gamepadCooldown = 0;
            gamepadCooldown -= GetFrameTime();

            if (gamepadCooldown <= 0)
            {
                if (axisX > config.gamepad_deadzone && selectedIndex % cols < cols - 1 && selectedIndex < (int)apps.size() - 1)
                {
                    selectedIndex++;
                    gamepadCooldown = 0.2f;
                }
                if (axisX < -config.gamepad_deadzone && selectedIndex % cols > 0)
                {
                    selectedIndex--;
                    gamepadCooldown = 0.2f;
                }
                if (axisY > config.gamepad_deadzone && selectedIndex + cols < (int)apps.size())
                {
                    selectedIndex += cols;
                    gamepadCooldown = 0.2f;
                }
                if (axisY < -config.gamepad_deadzone && selectedIndex - cols >= 0)
                {
                    selectedIndex -= cols;
                    gamepadCooldown = 0.2f;
                }
            }

            if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN))
            {
                LaunchApp(selectedIndex);
            }
        }

        // Scroll handling
        float wheel = GetMouseWheelMove();
        if (wheel != 0)
        {
            targetScrollY -= wheel * config.scroll_speed * 5;
        }

        // Ensure selected item is visible
        Rectangle selectedRect = GetCellRect(selectedIndex);
        int windowHeight = GetScreenHeight();
        if (selectedRect.y < config.scroll_padding)
        {
            targetScrollY -= (config.scroll_padding - selectedRect.y);
        }
        else if (selectedRect.y + config.cell_height > windowHeight - config.scroll_padding)
        {
            targetScrollY += (selectedRect.y + config.cell_height - windowHeight + config.scroll_padding);
        }

        // Clamp scroll
        targetScrollY = std::clamp(targetScrollY, 0.0f, maxScrollY);

        // Smooth scroll
        scrollY += (targetScrollY - scrollY) * config.smooth_scroll_factor;

        // Update animations
        for (int i = 0; i < (int)apps.size(); i++)
        {
            if (i == selectedIndex || i == hoveredIndex)
            {
                apps[i]->targetScale = config.selection_scale;
            }
            else
            {
                apps[i]->targetScale = 1.0f;
            }
            apps[i]->UpdateAnimation(config.animation_speed);
        }
    }

    void UpdateAnimations()
    {
        float deltaTime = GetFrameTime();
        animTimer += deltaTime;

        switch (animState)
        {
        case ANIM_FADE_IN:
            // Update fade alpha
            fadeAlpha = 1.0f - (animTimer / config.fade_in_duration);
            if (fadeAlpha < 0)
                fadeAlpha = 0;

            // Update app animations
            for (auto &app : apps)
            {
                if (animTimer > app->animDelay)
                {
                    app->UpdateFadeInAnimation(deltaTime, config.tile_animation_duration);
                }
            }

            // Check if fade in is complete
            if (animTimer > config.fade_in_duration + apps.size() * config.tile_stagger_delay + config.tile_animation_duration)
            {
                animState = ANIM_NORMAL;
                fadeAlpha = 0;
            }
            break;

        case ANIM_LAUNCHING:
        {
            float progress = animTimer / config.launch_animation_duration;
            if (progress > 1.0f)
                progress = 1.0f;

            // Calculate center point of launching app
            Rectangle launchRect = GetCellRect(launchingAppIndex);
            Vector2 centerPoint = {
                launchRect.x + launchRect.width / 2,
                launchRect.y + launchRect.height / 2};

            // Update each app's launch animation
            for (int i = 0; i < (int)apps.size(); i++)
            {
                apps[i]->UpdateLaunchAnimation(progress, i, centerPoint);
            }

            // Fade to black
            fadeAlpha = progress;

            // Launch the app when animation is complete
            if (progress >= 1.0f && !pendingLaunchCommand.empty())
            {
                int rc = system(pendingLaunchCommand.c_str());
                if (rc == -1)
                {
                    std::cerr << "system() failed: " << std::strerror(errno) << "\n";
                }
                else if (WIFEXITED(rc))
                {
                    int code = WEXITSTATUS(rc);
                    if (code != 0)
                    {
                        std::cerr << "Launcher shell exited with code " << code << "\n";
                    }
                }
                else if (WIFSIGNALED(rc))
                {
                    std::cerr << "Launcher shell killed by signal " << WTERMSIG(rc) << "\n";
                }
                pendingLaunchCommand.clear();
            }

            // Handle restore animation
            if (IsWindowFocused())
            {
                if (!wasFocusedLastFrame)
                {
                    animState = ANIM_FADE_IN;
                    animTimer = 0.0f;
                    InitializeAnimations();
                    StopMusicStream(music);
                    PlayMusicStream(music);
                }

                wasFocusedLastFrame = true;
            }
            else
            {
                wasFocusedLastFrame = false;
            }
        }
        break;

        case ANIM_NORMAL:
            // Normal state, no special animations
            break;
        }
    }

    void Draw()
    {
        int windowWidth = GetScreenWidth();
        int windowHeight = GetScreenHeight();

        BeginDrawing();
        ClearBackground(Color{220, 220, 220, 255}); // Light gray background

        // Draw gradient background
        DrawRectangleGradientV(0, 0, windowWidth, windowHeight,
                               Color{220, 220, 220, 255}, Color{200, 200, 200, 255}); // Light gradient

        // Draw message if no apps
        if (apps.empty())
        {
            const char *message = "No applications with valid icons found";
            Vector2 textSize = MeasureTextEx(font, message, 24, 1);
            DrawTextEx(font, message,
                       {windowWidth / 2.0f - textSize.x / 2, windowHeight / 2.0f - textSize.y / 2},
                       24, 1, Color{100, 100, 100, 255});
        }
        else
        {
            // Draw apps
            for (int i = 0; i < (int)apps.size(); i++)
            {
                Rectangle cellRect = GetCellRect(i);

                // Skip if outside visible area (only in normal state)
                if (animState == ANIM_NORMAL && (cellRect.y + config.cell_height < 0 || cellRect.y > windowHeight))
                    continue;

                float scale = apps[i]->scale;
                float opacity = apps[i]->opacity;
                bool isSelected = (i == selectedIndex || i == hoveredIndex);

                // Apply animation offsets
                float drawX = cellRect.x;
                float drawY = cellRect.y;

                if (animState == ANIM_FADE_IN)
                {
                    drawY += apps[i]->animOffset.y;
                }
                else if (animState == ANIM_LAUNCHING)
                {
                    drawX = apps[i]->animOffset.x - cellRect.width / 2;
                    drawY = apps[i]->animOffset.y - cellRect.height / 2;
                }

                // Draw selection highlight
                if (isSelected && animState == ANIM_NORMAL)
                {
                    Color highlightColor = {100, 150, 200, (unsigned char)(100 * opacity)}; // Light blue highlight
                    DrawRectangleRounded(
                        {drawX + 10, drawY + 10, cellRect.width - 20, cellRect.height - 20},
                        0.1f, 8, highlightColor);
                }

                // Draw icon with scaling
                float iconX = drawX + cellRect.width / 2;
                float iconY = drawY + config.cell_height / 2 - 20;
                float scaledSize = config.icon_size * scale;

                if (apps[i]->hasTexture)
                {
                    Color tint = {255, 255, 255, (unsigned char)(255 * opacity)};
                    DrawTexturePro(
                        apps[i]->texture,
                        {0, 0, (float)apps[i]->texture.width, (float)apps[i]->texture.height},
                        {iconX - scaledSize / 2, iconY - scaledSize / 2, scaledSize, scaledSize},
                        {0, 0}, 0, tint);
                }

                // Draw app name
                Vector2 textSize = MeasureTextEx(font, apps[i]->name.c_str(), 32, 1);
                float textX = drawX + cellRect.width / 2 - textSize.x / 2;
                float textY = iconY + scaledSize / 2 + 10;

                // Draw text shadow
                Color shadowColor = {50, 50, 50, (unsigned char)(32 * opacity)}; // Darker shadow
                Color textColor = {0, 0, 0, (unsigned char)(255 * opacity)};     // Black text
                DrawTextEx(font, apps[i]->name.c_str(), {textX + 1, textY + 1}, 32, 1, shadowColor);
                DrawTextEx(font, apps[i]->name.c_str(), {textX, textY}, 32, 1, textColor);
            }
        }

        // Draw UI elements only when not launching an app
        if (animState != ANIM_LAUNCHING)
        {
            DrawRectangleGradientV(0, 0, windowWidth, config.scroll_padding,
                                   Color{220, 220, 220, 255}, Color{220, 220, 220, 0});

            // Draw bottom gradient fade
            DrawRectangleGradientV(0, windowHeight - config.scroll_padding, windowWidth, config.scroll_padding,
                                   Color{220, 220, 220, 0}, Color{220, 220, 220, 255});

            // Draw title
            if (logoTexture.id)
            {
                float logoX = (windowWidth - logoTexture.width) / 2.0f;
                float logoY = 12;
                DrawTexture(logoTexture, logoX, logoY, WHITE);
            }

            // Draw scroll indicator if needed
            if (maxScrollY > 0 && !apps.empty())
            {
                float scrollPercent = scrollY / maxScrollY;
                float barHeight = 200;
                float indicatorHeight = 40;
                float indicatorY = 100 + scrollPercent * (barHeight - indicatorHeight);

                DrawRectangle(windowWidth - 10, 100, 4, barHeight, Color{150, 150, 150, 100});
                DrawRectangle(windowWidth - 10, indicatorY, 4, indicatorHeight, Color{50, 50, 50, 200});
            }
        }

        // Draw fade overlay
        if (fadeAlpha > 0)
        {
            DrawRectangle(0, 0, windowWidth, windowHeight,
                          Color{0, 0, 0, (unsigned char)(255 * fadeAlpha)});
        }

        EndDrawing();
    }

    void Run()
    {
        LoadApplications();

        while (!WindowShouldClose())
        {
            if (music.stream.buffer != nullptr)
            {
                UpdateMusicStream(music);
            }

            UpdateAnimations();
            HandleInput();
            Draw();
        }
    }

    Sound GetLoginSound() { return LoadSound(config.snd_login.c_str()); }
    const LauncherConfig &GetConfig() const { return config; }
};

int main()
{
    // Load configuration first
    LauncherConfig config = ConfigParser::LoadConfig(LauncherConfig::CONFIG_FILE);

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(config.initial_window_width, config.initial_window_height, "Dendy Launcher");
    InitAudioDevice();
    SetTargetFPS(60);

    {
        // Everything that owns GPU/audio resources lives inside this scope
        Sound fxLogin = LoadSound(config.snd_login.c_str());
        PlaySound(fxLogin);

        AppLauncher launcher;
        launcher.Run();

        UnloadSound(fxLogin); // unload while audio device is still open
        // AppLauncher destructor runs here (textures/fonts/music/sounds unloaded)
    }

    // Now it's safe to close devices/contexts
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
