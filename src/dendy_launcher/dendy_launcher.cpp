// app_launcher.cpp - Apple TV style application launcher for Linux

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
constexpr int INITIAL_WINDOW_WIDTH = 1280;
constexpr int INITIAL_WINDOW_HEIGHT = 720;
constexpr int MIN_GRID_COLS = 3;
constexpr int MAX_GRID_COLS = 10;
constexpr int ICON_SIZE = 128;
constexpr int ICON_PADDING = 40;
constexpr int TEXT_HEIGHT = 30;
constexpr int CELL_WIDTH = 200;
constexpr int CELL_HEIGHT = 200;
constexpr float SCROLL_SPEED = 15.0f;
constexpr float SMOOTH_SCROLL_FACTOR = 0.15f;
constexpr float GAMEPAD_DEADZONE = 0.25f;
constexpr float SELECTION_SCALE = 1.1f;
constexpr float ANIMATION_SPEED = 0.2f;

bool hasBeginning (std::string const &fullString, std::string const &beginning) {
    if (fullString.length() >= beginning.length()) {
        return (0 == fullString.compare(0, beginning.length(), beginning));
    } else {
        return false;
    }
}

bool hasEnding (std::string const &fullString, std::string const &ending) {
    if (fullString.length() >= ending.length()) {
        return (0 == fullString.compare (fullString.length() - ending.length(), ending.length(), ending));
    } else {
        return false;
    }
}

class AppEntry {
public:
    std::string name;
    std::string exec;
    std::string icon;
    Texture2D texture;
    bool hasTexture;
    float scale;
    float targetScale;
    
    AppEntry() : hasTexture(false), scale(1.0f), targetScale(1.0f) {}
    
    ~AppEntry() {
        if (hasTexture) {
            UnloadTexture(texture);
        }
    }
    
    // Disable copy to avoid texture issues
    AppEntry(const AppEntry&) = delete;
    AppEntry& operator=(const AppEntry&) = delete;
    
    // Enable move
    AppEntry(AppEntry&& other) noexcept 
        : name(std::move(other.name)),
          exec(std::move(other.exec)),
          icon(std::move(other.icon)),
          texture(other.texture),
          hasTexture(other.hasTexture),
          scale(other.scale),
          targetScale(other.targetScale) {
        other.hasTexture = false;
    }
    
    AppEntry& operator=(AppEntry&& other) noexcept {
        if (this != &other) {
            if (hasTexture) UnloadTexture(texture);
            name = std::move(other.name);
            exec = std::move(other.exec);
            icon = std::move(other.icon);
            texture = other.texture;
            hasTexture = other.hasTexture;
            scale = other.scale;
            targetScale = other.targetScale;
            other.hasTexture = false;
        }
        return *this;
    }
    
    void UpdateAnimation() {
        scale += (targetScale - scale) * ANIMATION_SPEED;
    }
};

class DesktopFileParser {
private:
    static std::string Trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\n\r");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\n\r");
        return str.substr(first, (last - first + 1));
    }
    
public:
    static std::unique_ptr<AppEntry> ParseFile(const fs::path& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) return nullptr;
        
        auto app = std::make_unique<AppEntry>();
        std::string line;
        bool inDesktopEntry = false;
        bool isValid = true;
        
        while (std::getline(file, line)) {
            line = Trim(line);
            
            if (line == "[Desktop Entry]") {
                inDesktopEntry = true;
            } else if (hasBeginning(line, "[")) {
                inDesktopEntry = false;
            } else if (inDesktopEntry && !line.empty()) {
                if (hasBeginning(line, "Name=") && app->name.empty()) {
                    app->name = line.substr(5);
                } else if (hasBeginning(line, "Exec=")) {
                    app->exec = line.substr(5);
                    // Remove field codes like %f, %F, %u, %U
                    size_t pos = app->exec.find(" %");
                    if (pos != std::string::npos) {
                        app->exec = app->exec.substr(0, pos);
                    }
                } else if (hasBeginning(line, "Icon=")) {
                    app->icon = line.substr(5);
                } else if (line == "NoDisplay=true" || line == "Hidden=true") {
                    isValid = false;
                    break;
                }
            }
        }
        
        if (!isValid || app->name.empty() || app->exec.empty()) {
            return nullptr;
        }
        
        return app;
    }
};

class IconLoader {
private:
    static std::vector<std::string> GetIconSearchPaths() {
        return {
            "/usr/share/icons/hicolor",
            "/usr/share/icons/gnome",
            "/usr/share/icons/Adwaita",
            "/usr/share/pixmaps"
        };
    }
    
    static std::vector<std::string> GetIconSizes() {
        return {"128x128", "256x256", "scalable", "64x64", "48x48"};
    }
    
public:
    static std::string FindIcon(const std::string& iconName) {
        // If it's already a full path
        if (hasEnding(iconName, "/") && fs::exists(iconName)) {
            return iconName;
        }
        
        std::vector<std::string> extensions = {".png", ".svg", ".xpm", ""};
        
        for (const auto& basePath : GetIconSearchPaths()) {
            // Direct pixmaps search
            if (hasEnding(basePath, "pixmaps")) {
                for (const auto& ext : extensions) {
                    std::string path = basePath + "/" + iconName + ext;
                    if (fs::exists(path)) return path;
                }
                continue;
            }
            
            // Themed icon search
            for (const auto& size : GetIconSizes()) {
                for (const auto& ext : extensions) {
                    std::string path = basePath + "/" + size + "/apps/" + iconName + ext;
                    if (fs::exists(path)) return path;
                }
            }
        }
        
        return "";
    }
    
    static Texture2D LoadIconTexture(const std::string& iconPath) {
        Texture2D tex = {0};
        
        if (iconPath.empty()) {
            // Create default icon
            Image img = GenImageColor(ICON_SIZE, ICON_SIZE, LIGHTGRAY);
            tex = LoadTextureFromImage(img);
            UnloadImage(img);
            return tex;
        }
        
        // Load image based on extension
        if (hasEnding(iconPath, ".svg")) {
            // For SVG, we'd need a library like librsvg, so use default for now
            Image img = GenImageColor(ICON_SIZE, ICON_SIZE, LIGHTGRAY);
            tex = LoadTextureFromImage(img);
            UnloadImage(img);
        } else {
            Image img = LoadImage(iconPath.c_str());
            if (img.data) {
                // Resize to standard icon size
                ImageResize(&img, ICON_SIZE, ICON_SIZE);
                tex = LoadTextureFromImage(img);
                UnloadImage(img);
            } else {
                // Fallback to default
                img = GenImageColor(ICON_SIZE, ICON_SIZE, LIGHTGRAY);
                tex = LoadTextureFromImage(img);
                UnloadImage(img);
            }
        }
        
        return tex;
    }
};

class AppLauncher {
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
    
    void LoadApplicationsFromDirectory(const fs::path& dir) {
        if (!fs::exists(dir) || !fs::is_directory(dir)) return;
        
        for (const auto& entry : fs::directory_iterator(dir)) {
            if (entry.path().extension() == ".desktop") {
                auto app = DesktopFileParser::ParseFile(entry.path());
                if (app) {
                    apps.push_back(std::move(app));
                }
            }
        }
    }
    
    void SortApplications() {
        std::sort(apps.begin(), apps.end(), 
            [](const std::unique_ptr<AppEntry>& a, const std::unique_ptr<AppEntry>& b) {
                return std::lexicographical_compare(
                    a->name.begin(), a->name.end(),
                    b->name.begin(), b->name.end(),
                    [](char c1, char c2) { return std::tolower(c1) < std::tolower(c2); }
                );
            });
    }
    
    void LoadIcons() {
        for (auto& app : apps) {
            std::string iconPath = IconLoader::FindIcon(app->icon);
            app->texture = IconLoader::LoadIconTexture(iconPath);
            app->hasTexture = true;
        }
    }
    
    int CalculateGridColumns(int windowWidth) const {
        int cols = windowWidth / CELL_WIDTH;
        return std::clamp(cols, MIN_GRID_COLS, MAX_GRID_COLS);
    }
    
    void UpdateMaxScroll() {
        int windowHeight = GetScreenHeight();
        int rows = ((int)apps.size() + currentGridCols - 1) / currentGridCols;
        maxScrollY = std::max(0.0f, (float)(rows * CELL_HEIGHT - windowHeight + 100));
    }
    
    Rectangle GetCellRect(int index) const {
        int windowWidth = GetScreenWidth();
        int row = index / currentGridCols;
        int col = index % currentGridCols;
        
        float gridWidth = currentGridCols * CELL_WIDTH;
        float x = (windowWidth - gridWidth) / 2 + col * CELL_WIDTH;
        float y = row * CELL_HEIGHT - scrollY + 50;
        
        return {x, y, (float)CELL_WIDTH, (float)CELL_HEIGHT};
    }
    
    void LaunchApp(int index) {
        if (index >= 0 && index < (int)apps.size()) {
            std::string command = apps[index]->exec + " &";
            system(command.c_str());
        }
    }
    
    void CheckWindowResize() {
        int windowWidth = GetScreenWidth();
        int windowHeight = GetScreenHeight();
        
        if (windowWidth != lastWindowWidth || windowHeight != lastWindowHeight) {
            lastWindowWidth = windowWidth;
            lastWindowHeight = windowHeight;
            
            // Recalculate grid columns
            int newGridCols = CalculateGridColumns(windowWidth);
            
            // If grid columns changed, adjust selected index to stay on same app
            if (newGridCols != currentGridCols && selectedIndex >= 0) {
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
                    lastWindowWidth(INITIAL_WINDOW_WIDTH), lastWindowHeight(INITIAL_WINDOW_HEIGHT) {
        font = LoadFontEx("assets/fonts/Inter-Regular.ttf", 20, nullptr, 0);
        if (!font.texture.id) {
            font = GetFontDefault();
        }
    }
    
    ~AppLauncher() {
        if (font.texture.id != GetFontDefault().texture.id) {
            UnloadFont(font);
        }
    }
    
    void LoadApplications() {
        apps.clear();
        
        // Load from standard directories
        LoadApplicationsFromDirectory("/usr/share/applications");
        LoadApplicationsFromDirectory("/usr/local/share/applications");
        
        // Load from user directory
        std::string home = getenv("HOME") ? getenv("HOME") : "";
        if (!home.empty()) {
            LoadApplicationsFromDirectory(home + "/.local/share/applications");
        }
        
        SortApplications();
        LoadIcons();
        UpdateMaxScroll();
    }
    
    void HandleInput() {
        hoveredIndex = -1;
        
        // Check for window resize
        CheckWindowResize();
        
        // Mouse input
        Vector2 mousePos = GetMousePosition();
        for (int i = 0; i < (int)apps.size(); i++) {
            Rectangle cellRect = GetCellRect(i);
            if (CheckCollisionPointRec(mousePos, cellRect)) {
                hoveredIndex = i;
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    LaunchApp(i);
                }
                break;
            }
        }
        
        // Keyboard navigation
        int cols = currentGridCols;
        if (IsKeyPressed(KEY_RIGHT) && selectedIndex % cols < cols - 1 && selectedIndex < (int)apps.size() - 1) {
            selectedIndex++;
        }
        if (IsKeyPressed(KEY_LEFT) && selectedIndex % cols > 0) {
            selectedIndex--;
        }
        if (IsKeyPressed(KEY_DOWN) && selectedIndex + cols < (int)apps.size()) {
            selectedIndex += cols;
        }
        if (IsKeyPressed(KEY_UP) && selectedIndex - cols >= 0) {
            selectedIndex -= cols;
        }
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
            LaunchApp(selectedIndex);
        }
        
        // Gamepad navigation
        if (IsGamepadAvailable(0)) {
            float axisX = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X);
            float axisY = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y);
            
            static float gamepadCooldown = 0;
            gamepadCooldown -= GetFrameTime();
            
            if (gamepadCooldown <= 0) {
                if (axisX > GAMEPAD_DEADZONE && selectedIndex % cols < cols - 1 && selectedIndex < (int)apps.size() - 1) {
                    selectedIndex++;
                    gamepadCooldown = 0.2f;
                }
                if (axisX < -GAMEPAD_DEADZONE && selectedIndex % cols > 0) {
                    selectedIndex--;
                    gamepadCooldown = 0.2f;
                }
                if (axisY > GAMEPAD_DEADZONE && selectedIndex + cols < (int)apps.size()) {
                    selectedIndex += cols;
                    gamepadCooldown = 0.2f;
                }
                if (axisY < -GAMEPAD_DEADZONE && selectedIndex - cols >= 0) {
                    selectedIndex -= cols;
                    gamepadCooldown = 0.2f;
                }
            }
            
            if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) {
                LaunchApp(selectedIndex);
            }
        }
        
        // Scroll handling
        float wheel = GetMouseWheelMove();
        if (wheel != 0) {
            targetScrollY -= wheel * SCROLL_SPEED * 5;
        }
        
        // Ensure selected item is visible
        Rectangle selectedRect = GetCellRect(selectedIndex);
        int windowHeight = GetScreenHeight();
        if (selectedRect.y < 50) {
            targetScrollY -= (50 - selectedRect.y);
        } else if (selectedRect.y + CELL_HEIGHT > windowHeight - 50) {
            targetScrollY += (selectedRect.y + CELL_HEIGHT - windowHeight + 50);
        }
        
        // Clamp scroll
        targetScrollY = std::clamp(targetScrollY, 0.0f, maxScrollY);
        
        // Smooth scroll
        scrollY += (targetScrollY - scrollY) * SMOOTH_SCROLL_FACTOR;
        
        // Update animations
        for (int i = 0; i < (int)apps.size(); i++) {
            if (i == selectedIndex || i == hoveredIndex) {
                apps[i]->targetScale = SELECTION_SCALE;
            } else {
                apps[i]->targetScale = 1.0f;
            }
            apps[i]->UpdateAnimation();
        }
    }
    
    void Draw() {
        int windowWidth = GetScreenWidth();
        int windowHeight = GetScreenHeight();
        
        BeginDrawing();
        ClearBackground(Color{20, 20, 25, 255});
        
        // Draw gradient background
        DrawRectangleGradientV(0, 0, windowWidth, windowHeight, 
            Color{20, 20, 25, 255}, Color{40, 40, 50, 255});
        
        // Draw apps
        for (int i = 0; i < (int)apps.size(); i++) {
            Rectangle cellRect = GetCellRect(i);
            
            // Skip if outside visible area
            if (cellRect.y + CELL_HEIGHT < 0 || cellRect.y > windowHeight) continue;
            
            float scale = apps[i]->scale;
            bool isSelected = (i == selectedIndex || i == hoveredIndex);
            
            // Draw selection highlight
            if (isSelected) {
                DrawRectangleRounded(
                    {cellRect.x + 10, cellRect.y + 10, cellRect.width - 20, cellRect.height - 20},
                    0.1f, 8, Color{60, 60, 70, 100}
                );
            }
            
            // Draw icon with scaling
            float iconX = cellRect.x + cellRect.width / 2;
            float iconY = cellRect.y + CELL_HEIGHT / 2 - 20;
            float scaledSize = ICON_SIZE * scale;
            
            if (apps[i]->hasTexture) {
                DrawTexturePro(
                    apps[i]->texture,
                    {0, 0, (float)apps[i]->texture.width, (float)apps[i]->texture.height},
                    {iconX - scaledSize/2, iconY - scaledSize/2, scaledSize, scaledSize},
                    {0, 0}, 0, WHITE
                );
            }
            
            // Draw app name
            Vector2 textSize = MeasureTextEx(font, apps[i]->name.c_str(), 18, 1);
            float textX = cellRect.x + cellRect.width / 2 - textSize.x / 2;
            float textY = iconY + scaledSize/2 + 10;
            
            // Draw text shadow
            DrawTextEx(font, apps[i]->name.c_str(), {textX + 1, textY + 1}, 18, 1, Color{0, 0, 0, 180});
            DrawTextEx(font, apps[i]->name.c_str(), {textX, textY}, 18, 1, WHITE);
        }
        
        // Draw top gradient fade
        DrawRectangleGradientV(0, 0, windowWidth, 50, 
            Color{20, 20, 25, 255}, Color{20, 20, 25, 0});
        
        // Draw bottom gradient fade
        DrawRectangleGradientV(0, windowHeight - 50, windowWidth, 50, 
            Color{20, 20, 25, 0}, Color{20, 20, 25, 255});
        
        // Draw title
        DrawTextEx(font, "Applications", {20, 15}, 28, 1, WHITE);
        
        // Draw grid info in corner (for debugging, remove if not needed)
        char info[64];
        snprintf(info, sizeof(info), "Grid: %dx%d", currentGridCols, 
                 ((int)apps.size() + currentGridCols - 1) / currentGridCols);
        DrawTextEx(font, info, {windowWidth - 150, 20}, 14, 1, Color{150, 150, 150, 150});
        
        // Draw scroll indicator if needed
        if (maxScrollY > 0) {
            float scrollPercent = scrollY / maxScrollY;
            float barHeight = 200;
            float indicatorHeight = 40;
            float indicatorY = 100 + scrollPercent * (barHeight - indicatorHeight);
            
            DrawRectangle(windowWidth - 10, 100, 4, barHeight, Color{100, 100, 100, 100});
            DrawRectangle(windowWidth - 10, indicatorY, 4, indicatorHeight, Color{200, 200, 200, 200});
        }
        
        EndDrawing();
    }
    
    void Run() {
        LoadApplications();
        
        while (!WindowShouldClose()) {
            HandleInput();
            Draw();
        }
    }
};

int main() {
    // Initialize window
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT, "Application Launcher");
    SetTargetFPS(60);
    
    // Create and run launcher
    AppLauncher launcher;
    launcher.Run();
    
    // Cleanup
    CloseWindow();
    
    return 0;
}