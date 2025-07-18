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

namespace fs = std::filesystem;

// Configuration constants
constexpr int INITIAL_WINDOW_WIDTH = 1920;
constexpr int INITIAL_WINDOW_HEIGHT = 1080;
constexpr int MIN_GRID_COLS = 3;
constexpr int MAX_GRID_COLS = 5;
constexpr int ICON_SIZE = 196;
constexpr int ICON_PADDING = 64;
constexpr int TOP_MARGIN = 128;
constexpr int BOTTOM_MARGIN = 100;
constexpr int SCROLL_PADDING = 50;
constexpr int TEXT_HEIGHT = 32;
constexpr int CELL_WIDTH = 300;
constexpr int CELL_HEIGHT = 300;
constexpr float SCROLL_SPEED = 15.0f;
constexpr float SMOOTH_SCROLL_FACTOR = 0.15f;
constexpr float GAMEPAD_DEADZONE = 0.25f;
constexpr float SELECTION_SCALE = 1.1f;
constexpr float ANIMATION_SPEED = 0.2f;
constexpr float FADE_IN_DURATION = 0.1f;
constexpr float TILE_STAGGER_DELAY = 0.03f;
constexpr float TILE_ANIMATION_DURATION = 0.5f;
constexpr float LAUNCH_ANIMATION_DURATION = 0.6f;

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

    void UpdateAnimation()
    {
        scale += (targetScale - scale) * ANIMATION_SPEED;
    }

    void UpdateFadeInAnimation(float deltaTime)
    {
        if (animProgress < 1.0f)
        {
            animProgress = std::min(1.0f, animProgress + deltaTime / TILE_ANIMATION_DURATION);

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

    void UpdateLaunchAnimation(float progress, int index, int totalApps, Vector2 centerPoint)
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
    static bool TryLoadIconTexture(const std::string &iconPath, Texture2D &tex)
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
                float scale = std::min((float)ICON_SIZE / img.width, (float)ICON_SIZE / img.height);
                int newWidth = img.width * scale;
                int newHeight = img.height * scale;

                ImageResize(&img, newWidth, newHeight);

                // Create a new image with padding if needed
                if (newWidth < ICON_SIZE || newHeight < ICON_SIZE)
                {
                    Image paddedImg = GenImageColor(ICON_SIZE, ICON_SIZE, BLANK);
                    int offsetX = (ICON_SIZE - newWidth) / 2;
                    int offsetY = (ICON_SIZE - newHeight) / 2;
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
    Music music = LoadMusicStream("/etc/dendy/assets/bg01.mp3");
    Sound fxMove = LoadSound("/etc/dendy/assets/move.wav");
    Sound fxSelect = LoadSound("/etc/dendy/assets/select.wav");
    Font fontBold = LoadFontEx("/etc/dendy/assets/fonts/Bogart-Black-trial.ttf", 96, 0, 250);
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
                           [](std::unique_ptr<AppEntry> &app)
                           {
                               std::string iconPath = IconLoader::FindIcon(app->icon);

                               if (iconPath.empty())
                               {
                                   std::cout << "No icon found for: " << app->name << " (icon: " << app->icon << ")" << std::endl;
                                   return true; // Remove this app
                               }

                               // Try to load the texture
                               bool success = IconLoader::TryLoadIconTexture(iconPath, app->texture);
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
            apps[i]->animDelay = i * TILE_STAGGER_DELAY;
            apps[i]->animProgress = 0.0f;
            apps[i]->opacity = 0.0f;
        }
    }

    int CalculateGridColumns(int windowWidth) const
    {
        int cols = windowWidth / CELL_WIDTH;
        return std::clamp(cols, MIN_GRID_COLS, MAX_GRID_COLS);
    }

    void UpdateMaxScroll()
    {
        int windowHeight = GetScreenHeight();
        int rows = ((int)apps.size() + currentGridCols - 1) / currentGridCols;
        float contentHeight = rows * CELL_HEIGHT + TOP_MARGIN + BOTTOM_MARGIN;
        maxScrollY = std::max(0.0f, contentHeight - windowHeight);
    }

    Rectangle GetCellRect(int index) const
    {
        int windowWidth = GetScreenWidth();
        int row = index / currentGridCols;
        int col = index % currentGridCols;

        float gridWidth = currentGridCols * CELL_WIDTH;
        float x = (windowWidth - gridWidth) / 2 + col * CELL_WIDTH;
        float y = row * CELL_HEIGHT - scrollY + TOP_MARGIN; // Use TOP_MARGIN constant

        return {x, y, (float)CELL_WIDTH, (float)CELL_HEIGHT};
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
                    currentGridCols(CalculateGridColumns(INITIAL_WINDOW_WIDTH)),
                    lastWindowWidth(INITIAL_WINDOW_WIDTH), lastWindowHeight(INITIAL_WINDOW_HEIGHT),
                    animState(ANIM_FADE_IN), animTimer(0.0f), fadeAlpha(1.0f),
                    launchingAppIndex(-1)
    {
        font = LoadFontEx("/etc/dendy/assets/fonts/Bogart-Medium-trial.ttf", 32, nullptr, 0);
        if (!font.texture.id)
        {
            font = GetFontDefault();
        }

        // Load logo
        Image logoImage = LoadImage("/etc/dendy/assets/logo.png");
        if (logoImage.data)
        {
            logoTexture = LoadTextureFromImage(logoImage);
            UnloadImage(logoImage);
        }

        // Start music
        PlayMusicStream(music);
    }

    ~AppLauncher()
    {
        if (font.texture.id != GetFontDefault().texture.id)
        {
            UnloadFont(font);
        }
    }

    void LoadApplications()
    {
        apps.clear();
        ResumeMusicStream(music);

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
                if (axisX > GAMEPAD_DEADZONE && selectedIndex % cols < cols - 1 && selectedIndex < (int)apps.size() - 1)
                {
                    selectedIndex++;
                    gamepadCooldown = 0.2f;
                }
                if (axisX < -GAMEPAD_DEADZONE && selectedIndex % cols > 0)
                {
                    selectedIndex--;
                    gamepadCooldown = 0.2f;
                }
                if (axisY > GAMEPAD_DEADZONE && selectedIndex + cols < (int)apps.size())
                {
                    selectedIndex += cols;
                    gamepadCooldown = 0.2f;
                }
                if (axisY < -GAMEPAD_DEADZONE && selectedIndex - cols >= 0)
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
            targetScrollY -= wheel * SCROLL_SPEED * 5;
        }

        // Ensure selected item is visible
        Rectangle selectedRect = GetCellRect(selectedIndex);
        int windowHeight = GetScreenHeight();
        if (selectedRect.y < SCROLL_PADDING)
        {
            targetScrollY -= (SCROLL_PADDING - selectedRect.y);
        }
        else if (selectedRect.y + CELL_HEIGHT > windowHeight - SCROLL_PADDING)
        {
            targetScrollY += (selectedRect.y + CELL_HEIGHT - windowHeight + SCROLL_PADDING);
        }

        // Clamp scroll
        targetScrollY = std::clamp(targetScrollY, 0.0f, maxScrollY);

        // Smooth scroll
        scrollY += (targetScrollY - scrollY) * SMOOTH_SCROLL_FACTOR;

        // Update animations
        for (int i = 0; i < (int)apps.size(); i++)
        {
            if (i == selectedIndex || i == hoveredIndex)
            {
                apps[i]->targetScale = SELECTION_SCALE;
            }
            else
            {
                apps[i]->targetScale = 1.0f;
            }
            apps[i]->UpdateAnimation();
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
            fadeAlpha = 1.0f - (animTimer / FADE_IN_DURATION);
            if (fadeAlpha < 0)
                fadeAlpha = 0;

            // Update app animations
            for (auto &app : apps)
            {
                if (animTimer > app->animDelay)
                {
                    app->UpdateFadeInAnimation(deltaTime);
                }
            }

            // Check if fade in is complete
            if (animTimer > FADE_IN_DURATION + apps.size() * TILE_STAGGER_DELAY + TILE_ANIMATION_DURATION)
            {
                animState = ANIM_NORMAL;
                fadeAlpha = 0;
            }
            break;

        case ANIM_LAUNCHING:
        {
            float progress = animTimer / LAUNCH_ANIMATION_DURATION;
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
                apps[i]->UpdateLaunchAnimation(progress, i, apps.size(), centerPoint);
            }

            // Fade to black
            fadeAlpha = progress;

            // Launch the app when animation is complete
            if (progress >= 1.0f && !pendingLaunchCommand.empty())
            {
                system(pendingLaunchCommand.c_str());
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
                if (animState == ANIM_NORMAL && (cellRect.y + CELL_HEIGHT < 0 || cellRect.y > windowHeight))
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
                float iconY = drawY + CELL_HEIGHT / 2 - 20;
                float scaledSize = ICON_SIZE * scale;

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
                Color textColor = {0, 0, 0, (unsigned char)(255 * opacity)};      // Black text
                DrawTextEx(font, apps[i]->name.c_str(), {textX + 1, textY + 1}, 32, 1, shadowColor);
                DrawTextEx(font, apps[i]->name.c_str(), {textX, textY}, 32, 1, textColor);
            }
        }

        // Draw UI elements only when not launching an app
        if (animState != ANIM_LAUNCHING)
        {
            DrawRectangleGradientV(0, 0, windowWidth, SCROLL_PADDING,
                                   Color{220, 220, 220, 255}, Color{220, 220, 220, 0});

            // Draw bottom gradient fade
            DrawRectangleGradientV(0, windowHeight - SCROLL_PADDING, windowWidth, SCROLL_PADDING,
                                   Color{220, 220, 220, 0}, Color{220, 220, 220, 255});

            // Draw title
            float logoX = (windowWidth - logoTexture.width) / 2.0f;
            float logoY = 12;

            DrawTexture(logoTexture, logoX, logoY, WHITE);

            // Draw grid info in corner (uncomment if desired)
            /*
            char info[64];
            snprintf(info, sizeof(info), "Grid: %dx%d", currentGridCols,
                    ((int)apps.size() + currentGridCols - 1) / currentGridCols);
            DrawTextEx(font, info, {windowWidth - 150, 20}, 14, 1, Color{50, 50, 50, 150}); // Darker gray
            */

            // Draw scroll indicator if needed
            if (maxScrollY > 0 && !apps.empty())
            {
                float scrollPercent = scrollY / maxScrollY;
                float barHeight = 200;
                float indicatorHeight = 40;
                float indicatorY = 100 + scrollPercent * (barHeight - indicatorHeight);

                DrawRectangle(windowWidth - 10, 100, 4, barHeight, Color{150, 150, 150, 100});           // Darker scroll bar
                DrawRectangle(windowWidth - 10, indicatorY, 4, indicatorHeight, Color{50, 50, 50, 200}); // Darker indicator
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
};

int main()
{
    // Initialize window
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT, "Dendy Launcher");
    InitAudioDevice();
    SetTargetFPS(60);

    Sound fxLogin = LoadSound("/etc/dendy/assets/login.wav");
    PlaySound(fxLogin);

    // Create and run launcher
    AppLauncher launcher;
    launcher.Run();

    // Cleanup
    CloseWindow();

    return 0;
}