#include "gpu_solver.hpp"
#include "grid.hpp"
#include <iostream>
#include <chrono>
#include <string>
#include <iomanip>
#include <ctime>

// Helper function to get current timestamp
std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

// Helper function to format FPS
std::string formatFPS(float fps) {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(3) << fps;
    return ss.str();
}

int main() {
    try {
        // Print initialization information
        std::cout << "Navier-Stokes GPU Solver" << std::endl;
        std::cout << "Initialization Time (UTC): " << getCurrentTimestamp() << std::endl;
        std::cout << "User: " << "SamarthGupta23" << std::endl;
        std::cout << std::string(50, '-') << std::endl;

        // Grid dimensions
        const int gridWidth = 512;
        const int gridHeight = 512;

        std::cout << "Configuration:" << std::endl;
        std::cout << "Grid Size: " << gridWidth << "x" << gridHeight << std::endl;

        // Initialize GPU solver
        std::cout << "Initializing GPU solver..." << std::endl;
        GPUSolver gpuSolver(gridWidth, gridHeight);
        if (!gpuSolver.initialize()) {
            std::cerr << "Failed to initialize GPU solver" << std::endl;
            return 1;
        }

        // Initialize timing variables
        auto lastTime = std::chrono::high_resolution_clock::now();
        auto lastFPSUpdate = lastTime;
        int frameCount = 0;
        float currentFPS = 0.0f;

        std::cout << "\nSimulation Controls:" << std::endl;
        std::cout << "- ESC: Exit simulation" << std::endl;
        std::cout << "- Left Mouse Button: Add forces" << std::endl;
        std::cout << "\nStarting simulation loop..." << std::endl;
        std::cout << std::string(50, '-') << std::endl;

        // Main simulation loop
        while (!gpuSolver.shouldClose()) {
            // Timing
            auto currentTime = std::chrono::high_resolution_clock::now();
            float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
            lastTime = currentTime;

            // Handle input
            gpuSolver.pollEvents();

            // Check for ESC key
            if (glfwGetKey(gpuSolver.getWindow(), GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                break;
            }

            // Handle mouse input for force injection
            static bool mousePressed = false;
            static double lastMouseX = 0, lastMouseY = 0;

            double mouseX, mouseY;
            glfwGetCursorPos(gpuSolver.getWindow(), &mouseX, &mouseY);

            // Convert screen coordinates to grid coordinates
            int gridX = static_cast<int>((mouseX / gpuSolver.getWindowWidth()) * gridWidth);
            int gridY = static_cast<int>((mouseY / gpuSolver.getWindowHeight()) * gridHeight);

            if (glfwGetMouseButton(gpuSolver.getWindow(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
                if (mousePressed) {
                    // Calculate force based on mouse movement
                    float forceX = static_cast<float>(mouseX - lastMouseX) * 0.001f;
                    float forceY = static_cast<float>(mouseY - lastMouseY) * 0.001f;
                    gpuSolver.addForce(gridX, gridY, forceX, forceY);
                }
                mousePressed = true;
            } else {
                mousePressed = false;
            }

            lastMouseX = mouseX;
            lastMouseY = mouseY;

            // Run simulation steps
            gpuSolver.applyForces();
            gpuSolver.diffuse();
            gpuSolver.advect();
            gpuSolver.project();

            // Render
            gpuSolver.render();

            // FPS calculation and display
            frameCount++;
            auto timeSinceLastFPSUpdate = std::chrono::duration<float>(currentTime - lastFPSUpdate).count();

            if (timeSinceLastFPSUpdate >= 1.0f) {  // Update FPS every second
                currentFPS = frameCount / timeSinceLastFPSUpdate;
                std::cout << "FPS: " << formatFPS(currentFPS) << std::endl;

                frameCount = 0;
                lastFPSUpdate = currentTime;
            }
        }

        std::cout << std::string(50, '-') << std::endl;
        std::cout << "Simulation ended at: " << getCurrentTimestamp() << std::endl;

        // Cleanup is handled by GPUSolver's destructor
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        std::cerr << "Error occurred at: " << getCurrentTimestamp() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error occurred at: " << getCurrentTimestamp() << std::endl;
        return 1;
    }
}