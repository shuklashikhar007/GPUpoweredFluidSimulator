#include "raylib_visualizer.hpp"
#include <iostream>

RaylibVisualizer::RaylibVisualizer(int width, int height)
    : windowWidth(width), windowHeight(height), currentFrame(0), totalFrames(0), isPlaying(true), frameRate(30.0f) {
}

RaylibVisualizer::~RaylibVisualizer() {
    cleanup();
}

bool RaylibVisualizer::initialize() {
    if (!InitWindow(windowWidth, windowHeight, "Navier Stokes Visualizer")) {
        std::cerr << "Failed to initialize raylib window" << std::endl;
        return false;
    }
    SetTargetFPS(60);

    camera = { 0 };
    camera.target = { 0.0f, 0.0f };
    camera.offset = { 0.0f, 0.0f };
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;

    lastFrameTime = std::chrono::steady_clock::now();
    return true;
}

void RaylibVisualizer::cleanup() {
    if (IsWindowReady()) CloseWindow();
}

Color RaylibVisualizer::velocityToColor(const Vec& velocity) {
    double mag = velocity.magnitude();
    // Clamp and map
    double v = std::min(1.0, mag / 5.0); // assume velocities ~ [0,5]
    unsigned char intensity = static_cast<unsigned char>(v * 255);
    return Color{ intensity, static_cast<unsigned char>(255 - intensity), 128, 255 };
}

void RaylibVisualizer::renderVelocityField(const std::vector<std::vector<Vec>>& frame) {
    // Create an image and texture sized to grid
    Image img = GenImageColor(gridWidth, gridHeight, BLANK);
    Color* pixels = (Color*)MemAlloc(gridWidth * gridHeight * sizeof(Color));

    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            Vec v = frame[y][x];
            Color c = velocityToColor(v);
            pixels[y * gridWidth + x] = c;
        }
    }

    // Copy pixels into image
    memcpy(img.data, pixels, gridWidth * gridHeight * sizeof(Color));
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    MemFree(pixels);

    // Draw scaled to window
    float scaleX = (float)windowWidth / (float)gridWidth;
    float scaleY = (float)windowHeight / (float)gridHeight;
    DrawTextureEx(tex, {0,0}, 0.0f, fmin(scaleX, scaleY), WHITE);

    UnloadTexture(tex);
}

bool RaylibVisualizer::loadFramesFromFile(const std::string& filename) {
    grid g;
    // Use grid::readFramesFromFile to populate generatedFrames
    g.readFramesFromFile(filename);
    if (g.generatedFrames.empty()) return false;

    frames = std::move(g.generatedFrames);
    totalFrames = static_cast<int>(frames.size());
    gridHeight = frames[0].size();
    gridWidth = frames[0][0].size();
    return true;
}

void RaylibVisualizer::drawUI() {
    DrawText("Space: Play/Pause  Left/Right: Prev/Next  +/-: FPS", 10, 10, 12, RAYWHITE);
    DrawText(TextFormat("Frame: %d / %d", currentFrame + 1, totalFrames), 10, 30, 12, RAYWHITE);
    DrawText(TextFormat("FPS: %.1f", frameRate), 10, 50, 12, RAYWHITE);
}

void RaylibVisualizer::playPause() { isPlaying = !isPlaying; }
void RaylibVisualizer::nextFrame() { currentFrame = (currentFrame + 1) % totalFrames; }
void RaylibVisualizer::previousFrame() { currentFrame = (currentFrame - 1 + totalFrames) % totalFrames; }
void RaylibVisualizer::setFrameRate(float fps) { frameRate = fps; }

void RaylibVisualizer::run() {
    if (frames.empty()) {
        std::cerr << "No frames loaded" << std::endl;
        return;
    }

    while (!WindowShouldClose()) {
        // Input
        if (IsKeyPressed(KEY_SPACE)) playPause();
        if (IsKeyPressed(KEY_RIGHT)) nextFrame();
        if (IsKeyPressed(KEY_LEFT)) previousFrame();
        if (IsKeyPressed(KEY_KP_ADD) || IsKeyPressed(KEY_EQUAL)) frameRate += 1.0f;
        if (IsKeyPressed(KEY_KP_SUBTRACT) || IsKeyPressed(KEY_MINUS)) frameRate = std::max(1.0f, frameRate - 1.0f);

        // Timing
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - lastFrameTime).count();
        if (isPlaying && elapsed >= 1.0 / frameRate) {
            currentFrame = (currentFrame + 1) % totalFrames;
            lastFrameTime = now;
        }

        BeginDrawing();
        ClearBackground(BLACK);

        renderVelocityField(frames[currentFrame]);
        drawUI();

        EndDrawing();
    }
}
