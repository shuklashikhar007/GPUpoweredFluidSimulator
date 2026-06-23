#pragma once

#include "grid.hpp"
#include <raylib.h>
#include <vector>
#include <fstream>
#include <chrono>

class RaylibVisualizer {
private:
    int windowWidth, windowHeight;
    int gridWidth, gridHeight;
    
    // Raylib resources
    RenderTexture2D renderTexture;
    Camera2D camera;
    
    // Simulation data
    std::vector<std::vector<std::vector<Vec>>> frames;
    int currentFrame;
    int totalFrames;
    
    // Animation control
    bool isPlaying;
    float frameRate;
    std::chrono::steady_clock::time_point lastFrameTime;
    
    // Rendering
    void renderVelocityField(const std::vector<std::vector<Vec>>& frame);
    Color velocityToColor(const Vec& velocity);
    void drawUI();
    
    // File I/O
    bool loadFramesFromFile(const std::string& filename);
    
public:
    RaylibVisualizer(int width = 800, int height = 600);
    ~RaylibVisualizer();
    
    bool initialize();
    void run();
    void cleanup();
    
    // Controls
    void playPause();
    void nextFrame();
    void previousFrame();
    void setFrameRate(float fps);
};
