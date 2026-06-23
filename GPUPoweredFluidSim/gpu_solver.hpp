#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <string>
#include "grid.hpp"
#include "shader_manager.hpp"

class GPUSolver {
private:
    // OpenGL context and window
    ShaderManager shaderManager;
    GLFWwindow* window;
    int windowWidth, windowHeight;

    // GPU textures
    GLuint velocityTexture[2];  // Ping-pong buffers for velocity
    GLuint velocityBefore;      // For diffusion (stores "before" state)
    GLuint pressureTexture[2];  // Ping-pong buffers for pressure
    GLuint divergenceTexture;   // Divergence field

    // Compute shaders
    GLuint advectionShader;
    GLuint diffusionShader;
    GLuint projectionShader;
    GLuint projectionGradientShader;
    GLuint boundaryShader;
    GLuint forceShader;

    // Shader programs
    GLuint advectionProgram;
    GLuint diffusionProgram;
    GLuint projectionProgram;
    GLuint projectionGradientProgram;
    GLuint boundaryProgram;
    GLuint forceProgram;

    // Display rendering
    GLuint displayVAO;
    GLuint displayVBO;
    GLuint displayTexture;
    GLuint displayShaderProgram;

    // Grid dimensions
    int gridWidth, gridHeight;

    // Simulation parameters
    float timeStep;
    float viscosity;
    float alpha;

    // Current buffer index (for ping-pong)
    int currentBuffer;

    // Helper functions
    GLuint createTexture(int width, int height, GLenum format);
    void swapBuffers();
    void applyPressureGradient();
    void checkGLError(const char* context);
    bool initializeShaders();
    bool initializeDisplayShader();
    bool initializeTextures();
    bool validateShaderProgram(GLuint program, const char* name);

public:
    GPUSolver(int width, int height);
    ~GPUSolver();

    // Initialization and cleanup
    bool initialize();
    void cleanup();

    // GPU simulation steps
    void applyForces();
    void diffuse();
    void advect();
    void project();

    // Data transfer
    void uploadVelocityData(const std::vector<std::vector<Vec>>& velocities);
    void downloadVelocityData(std::vector<std::vector<Vec>>& velocities);

    // Rendering
    void render();
    bool shouldClose() const { return glfwWindowShouldClose(window); }
    void pollEvents() { glfwPollEvents(); }

    // User interaction
    void addForce(int x, int y, float fx, float fy);
    void addDye(int x, int y, float intensity);

    // Getters
    GLFWwindow* getWindow() const { return window; }
    int getWindowWidth() const { return windowWidth; }
    int getWindowHeight() const { return windowHeight; }
};