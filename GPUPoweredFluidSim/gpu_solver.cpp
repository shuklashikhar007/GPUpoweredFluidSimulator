#include "gpu_solver.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <cmath>

void GPUSolver::checkGLError(const char* context) {
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        std::cerr << "OpenGL Error in " << context << ": " << err;
        switch(err) {
            case GL_INVALID_ENUM: std::cerr << " (INVALID_ENUM)"; break;
            case GL_INVALID_VALUE: std::cerr << " (INVALID_VALUE)"; break;
            case GL_INVALID_OPERATION: std::cerr << " (INVALID_OPERATION)"; break;
            case GL_OUT_OF_MEMORY: std::cerr << " (OUT_OF_MEMORY)"; break;
            case GL_INVALID_FRAMEBUFFER_OPERATION: std::cerr << " (INVALID_FRAMEBUFFER_OPERATION)"; break;
        }
        std::cerr << std::endl;
    }
}

GPUSolver::GPUSolver(int width, int height)
    : window(nullptr), windowWidth(800), windowHeight(600),
      gridWidth(width), gridHeight(height), currentBuffer(0) {

    velocityTexture[0] = velocityTexture[1] = velocityBefore = 0;
    pressureTexture[0] = pressureTexture[1] = divergenceTexture = 0;

    advectionShader = diffusionShader = projectionShader = 0;
    projectionGradientShader = boundaryShader = forceShader = 0;

    advectionProgram = diffusionProgram = projectionProgram = 0;
    projectionGradientProgram = boundaryProgram = forceProgram = 0;

    displayVAO = displayVBO = displayTexture = 0;
    displayShaderProgram = 0;

    timeStep = 0.2f;
    viscosity = 30.0f;
    alpha = viscosity * timeStep / (1.0f * 1.0f);
}

GPUSolver::~GPUSolver() {
    cleanup();
}

bool GPUSolver::initializeShaders() {
    std::cout << "\n=== Shader Initialization Debug ===\n";
    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
    std::cout << "GPU Vendor: " << glGetString(GL_VENDOR) << std::endl;
    std::cout << "GPU Renderer: " << glGetString(GL_RENDERER) << std::endl;

    // Force Shader
    std::cout << "\nCreating Force shader..." << std::endl;
    forceShader = shaderManager.createComputeShader("force", ShaderManager::FORCE_SHADER_SOURCE);
    if (forceShader == 0) return false;
    forceProgram = shaderManager.createComputeProgram("force", forceShader);
    if (forceProgram == 0) return false;

    // Diffusion Shader
    std::cout << "\nCreating Diffusion shader..." << std::endl;
    diffusionShader = shaderManager.createComputeShader("diffusion", ShaderManager::DIFFUSION_SHADER_SOURCE);
    if (diffusionShader == 0) return false;
    diffusionProgram = shaderManager.createComputeProgram("diffusion", diffusionShader);
    if (diffusionProgram == 0) return false;

    // Advection Shader
    std::cout << "\nCreating Advection shader..." << std::endl;
    advectionShader = shaderManager.createComputeShader("advection", ShaderManager::ADVECTION_SHADER_SOURCE);
    if (advectionShader == 0) return false;
    advectionProgram = shaderManager.createComputeProgram("advection", advectionShader);
    if (advectionProgram == 0) return false;

    // Projection Shader
    std::cout << "\nCreating Projection shader..." << std::endl;
    projectionShader = shaderManager.createComputeShader("projection", ShaderManager::PROJECTION_SHADER_SOURCE);
    if (projectionShader == 0) return false;
    projectionProgram = shaderManager.createComputeProgram("projection", projectionShader);
    if (projectionProgram == 0) return false;

    // Projection Gradient Shader
    std::cout << "\nCreating Projection Gradient shader..." << std::endl;
    projectionGradientShader = shaderManager.createComputeShader("projection_gradient", ShaderManager::PROJECTION_GRADIENT_SHADER_SOURCE);
    if (projectionGradientShader == 0) return false;
    projectionGradientProgram = shaderManager.createComputeProgram("projection_gradient", projectionGradientShader);
    if (projectionGradientProgram == 0) return false;

    // Create display shader for rendering
    if (!initializeDisplayShader()) {
        std::cerr << "Failed to initialize display shader" << std::endl;
        return false;
    }

    // Print active uniforms for each program
    std::cout << "\n=== Shader Uniform Status ===\n";
    for (const auto& [name, program] : {
        std::make_pair("Force", forceProgram),
        std::make_pair("Diffusion", diffusionProgram),
        std::make_pair("Advection", advectionProgram),
        std::make_pair("Projection", projectionProgram),
        std::make_pair("ProjectionGradient", projectionGradientProgram)
    }) {
        std::cout << "\nChecking " << name << " program uniforms:" << std::endl;
        GLint numUniforms = 0;
        glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &numUniforms);
        for (GLint i = 0; i < numUniforms; i++) {
            GLchar uniformName[64];
            GLint size;
            GLenum type;
            glGetActiveUniform(program, i, sizeof(uniformName), nullptr, &size, &type, uniformName);
            GLint location = glGetUniformLocation(program, uniformName);
            std::cout << "  " << uniformName << " (loc: " << location << ")" << std::endl;
        }
    }

    return true;
}

bool GPUSolver::initializeDisplayShader() {
    const char* vertexShaderSource = R"(
        #version 430 core
        layout (location = 0) in vec2 aPos;
        layout (location = 1) in vec2 aTexCoord;
        out vec2 TexCoord;
        void main() {
            gl_Position = vec4(aPos, 0.0, 1.0);
            TexCoord = aTexCoord;
        }
    )";

    const char* fragmentShaderSource = R"(
        #version 430 core
        in vec2 TexCoord;
        out vec4 FragColor;
        uniform sampler2D displayTexture;
        void main() {
            FragColor = texture(displayTexture, TexCoord);
        }
    )";

    // Compile vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(vertexShader);

    GLint success;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        std::cerr << "Vertex shader compilation failed:\n" << infoLog << std::endl;
        return false;
    }

    // Compile fragment shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        std::cerr << "Fragment shader compilation failed:\n" << infoLog << std::endl;
        return false;
    }

    // Link shader program
    displayShaderProgram = glCreateProgram();
    glAttachShader(displayShaderProgram, vertexShader);
    glAttachShader(displayShaderProgram, fragmentShader);
    glLinkProgram(displayShaderProgram);

    glGetProgramiv(displayShaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(displayShaderProgram, 512, nullptr, infoLog);
        std::cerr << "Display shader program linking failed:\n" << infoLog << std::endl;
        return false;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Create VAO and VBO for fullscreen quad
    float quadVertices[] = {
        // positions   // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };

    glGenVertexArrays(1, &displayVAO);
    glGenBuffers(1, &displayVBO);

    glBindVertexArray(displayVAO);
    glBindBuffer(GL_ARRAY_BUFFER, displayVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Texture coordinate attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    // Create display texture
    glGenTextures(1, &displayTexture);
    glBindTexture(GL_TEXTURE_2D, displayTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, windowWidth, windowHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);

    std::cout << "Display shader initialized successfully" << std::endl;
    return true;
}

bool GPUSolver::initializeTextures() {
    std::cout << "\nInitializing textures..." << std::endl;

    velocityTexture[0] = createTexture(gridWidth, gridHeight, GL_RG32F);
    if (!velocityTexture[0]) return false;

    velocityTexture[1] = createTexture(gridWidth, gridHeight, GL_RG32F);
    if (!velocityTexture[1]) return false;

    velocityBefore = createTexture(gridWidth, gridHeight, GL_RG32F);
    if (!velocityBefore) return false;

    pressureTexture[0] = createTexture(gridWidth, gridHeight, GL_R32F);
    if (!pressureTexture[0]) return false;

    pressureTexture[1] = createTexture(gridWidth, gridHeight, GL_R32F);
    if (!pressureTexture[1]) return false;

    divergenceTexture = createTexture(gridWidth, gridHeight, GL_R32F);
    if (!divergenceTexture) return false;

    std::cout << "All textures initialized successfully" << std::endl;
    return true;
}

bool GPUSolver::initialize() {
    std::cout << "\nInitializing GPU solver..." << std::endl;

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(windowWidth, windowHeight, "Navier-Stokes GPU Solver", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW: " << glewGetErrorString(err) << std::endl;
        return false;
    }

    while (glGetError() != GL_NO_ERROR);

    std::cout << "\nOpenGL Information:" << std::endl;
    std::cout << "  Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "  GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
    std::cout << "  Vendor: " << glGetString(GL_VENDOR) << std::endl;
    std::cout << "  Renderer: " << glGetString(GL_RENDERER) << std::endl;

    if (!GLEW_ARB_compute_shader) {
        std::cerr << "Compute shaders not supported!" << std::endl;
        return false;
    }

    if (!initializeTextures()) {
        cleanup();
        return false;
    }

    if (!initializeShaders()) {
        cleanup();
        return false;
    }

    glViewport(0, 0, windowWidth, windowHeight);

    std::cout << "\nGPU Solver initialized successfully!" << std::endl;
    std::cout << "Grid size: " << gridWidth << "x" << gridHeight << std::endl;
    std::cout << "Alpha (viscosity param): " << alpha << std::endl;
    std::cout << "Time step: " << timeStep << std::endl;

    return true;
}

GLuint GPUSolver::createTexture(int width, int height, GLenum format) {
    GLuint texture;
    glGenTextures(1, &texture);
    if (texture == 0) {
        std::cerr << "Failed to generate texture" << std::endl;
        return 0;
    }

    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0,
                 (format == GL_RG32F) ? GL_RG : GL_RED, GL_FLOAT, nullptr);

    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "Error creating texture: " << error << std::endl;
        glDeleteTextures(1, &texture);
        return 0;
    }

    return texture;
}

void GPUSolver::cleanup() {
    if (velocityTexture[0]) glDeleteTextures(1, &velocityTexture[0]);
    if (velocityTexture[1]) glDeleteTextures(1, &velocityTexture[1]);
    if (velocityBefore) glDeleteTextures(1, &velocityBefore);
    if (pressureTexture[0]) glDeleteTextures(1, &pressureTexture[0]);
    if (pressureTexture[1]) glDeleteTextures(1, &pressureTexture[1]);
    if (divergenceTexture) glDeleteTextures(1, &divergenceTexture);
    if (displayTexture) glDeleteTextures(1, &displayTexture);

    if (displayVAO) glDeleteVertexArrays(1, &displayVAO);
    if (displayVBO) glDeleteBuffers(1, &displayVBO);

    if (advectionShader) glDeleteShader(advectionShader);
    if (diffusionShader) glDeleteShader(diffusionShader);
    if (projectionShader) glDeleteShader(projectionShader);
    if (projectionGradientShader) glDeleteShader(projectionGradientShader);
    if (boundaryShader) glDeleteShader(boundaryShader);
    if (forceShader) glDeleteShader(forceShader);

    if (advectionProgram) glDeleteProgram(advectionProgram);
    if (diffusionProgram) glDeleteProgram(diffusionProgram);
    if (projectionProgram) glDeleteProgram(projectionProgram);
    if (projectionGradientProgram) glDeleteProgram(projectionGradientProgram);
    if (boundaryProgram) glDeleteProgram(boundaryProgram);
    if (forceProgram) glDeleteProgram(forceProgram);
    if (displayShaderProgram) glDeleteProgram(displayShaderProgram);

    if (window) {
        glfwDestroyWindow(window);
        window = nullptr;
    }
    glfwTerminate();
}

void GPUSolver::swapBuffers() {
    currentBuffer = 1 - currentBuffer;
}

void GPUSolver::applyForces() {
    glUseProgram(forceProgram);
    checkGLError("Force: glUseProgram");

    glBindImageTexture(0, velocityTexture[currentBuffer], 0, GL_FALSE, 0, GL_READ_WRITE, GL_RG32F);
    checkGLError("Force: bind texture");

    glUniform1f(glGetUniformLocation(forceProgram, "timeStep"), timeStep);
    glUniform1i(glGetUniformLocation(forceProgram, "width"), gridWidth);
    glUniform1i(glGetUniformLocation(forceProgram, "height"), gridHeight);
    checkGLError("Force: set uniforms");

    glDispatchCompute((gridWidth + 15) / 16, (gridHeight + 15) / 16, 1);
    checkGLError("Force: dispatch");

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

void GPUSolver::diffuse() {
    glCopyImageSubData(velocityTexture[currentBuffer], GL_TEXTURE_2D, 0, 0, 0, 0,
                      velocityBefore, GL_TEXTURE_2D, 0, 0, 0, 0,
                      gridWidth, gridHeight, 1);
    checkGLError("Diffusion: copy texture");

    for (int iter = 0; iter < 15; iter++) {
        glUseProgram(diffusionProgram);
        checkGLError("Diffusion: use program");

        glBindImageTexture(0, velocityTexture[1-currentBuffer], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG32F);
        glBindImageTexture(1, velocityTexture[currentBuffer], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RG32F);
        glBindImageTexture(2, velocityBefore, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RG32F);
        checkGLError("Diffusion: bind textures");

        glUniform1f(glGetUniformLocation(diffusionProgram, "alpha"), alpha);
        glUniform1i(glGetUniformLocation(diffusionProgram, "width"), gridWidth);
        glUniform1i(glGetUniformLocation(diffusionProgram, "height"), gridHeight);
        checkGLError("Diffusion: set uniforms");

        glDispatchCompute((gridWidth + 15) / 16, (gridHeight + 15) / 16, 1);
        checkGLError("Diffusion: dispatch");

        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        swapBuffers();
    }
}

void GPUSolver::advect() {
    glUseProgram(advectionProgram);
    checkGLError("Advection: use program");

    glBindImageTexture(0, velocityTexture[1-currentBuffer], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG32F);
    glBindImageTexture(1, velocityTexture[currentBuffer], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RG32F);
    checkGLError("Advection: bind textures");

    glUniform1f(glGetUniformLocation(advectionProgram, "timeStep"), timeStep);
    glUniform1i(glGetUniformLocation(advectionProgram, "width"), gridWidth);
    glUniform1i(glGetUniformLocation(advectionProgram, "height"), gridHeight);
    checkGLError("Advection: set uniforms");

    glDispatchCompute((gridWidth + 15) / 16, (gridHeight + 15) / 16, 1);
    checkGLError("Advection: dispatch");

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    swapBuffers();
}

void GPUSolver::project() {
    // Step 1: Compute divergence
    glUseProgram(projectionProgram);
    checkGLError("Projection: use program");

    glBindImageTexture(0, velocityTexture[currentBuffer], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RG32F);
    glBindImageTexture(1, pressureTexture[0], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
    glBindImageTexture(2, pressureTexture[0], 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32F);
    glBindImageTexture(3, divergenceTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
    checkGLError("Projection: bind textures");

    glUniform1i(glGetUniformLocation(projectionProgram, "mode"), 0);
    glUniform1i(glGetUniformLocation(projectionProgram, "width"), gridWidth);
    glUniform1i(glGetUniformLocation(projectionProgram, "height"), gridHeight);
    glUniform1f(glGetUniformLocation(projectionProgram, "timeStep"), timeStep);
    checkGLError("Projection: set uniforms");

    glDispatchCompute((gridWidth + 15) / 16, (gridHeight + 15) / 16, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    checkGLError("Projection: divergence");

    // Step 2: Pressure solve (Red-Black Gauss-Seidel)
    int pressureBuffer = 0;
    for (int iter = 0; iter < 20; iter++) {
        // Red phase
        glUniform1i(glGetUniformLocation(projectionProgram, "mode"), 1);
        glBindImageTexture(1, pressureTexture[1-pressureBuffer], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
        glBindImageTexture(2, pressureTexture[pressureBuffer], 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32F);

        glDispatchCompute((gridWidth + 15) / 16, (gridHeight + 15) / 16, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        pressureBuffer = 1 - pressureBuffer;

        // Black phase
        glUniform1i(glGetUniformLocation(projectionProgram, "mode"), 2);
        glBindImageTexture(1, pressureTexture[1-pressureBuffer], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
        glBindImageTexture(2, pressureTexture[pressureBuffer], 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32F);

        glDispatchCompute((gridWidth + 15) / 16, (gridHeight + 15) / 16, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        pressureBuffer = 1 - pressureBuffer;
    }

    // Step 3: Subtract pressure gradient
    glUseProgram(projectionGradientProgram);
    checkGLError("Projection: gradient program");

    glBindImageTexture(0, velocityTexture[currentBuffer], 0, GL_FALSE, 0, GL_READ_WRITE, GL_RG32F);
    glBindImageTexture(1, pressureTexture[pressureBuffer], 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32F);
    checkGLError("Projection: gradient bind");

    glUniform1i(glGetUniformLocation(projectionGradientProgram, "width"), gridWidth);
    glUniform1i(glGetUniformLocation(projectionGradientProgram, "height"), gridHeight);
    glUniform1f(glGetUniformLocation(projectionGradientProgram, "timeStep"), timeStep);
    checkGLError("Projection: gradient uniforms");

    glDispatchCompute((gridWidth + 15) / 16, (gridHeight + 15) / 16, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    checkGLError("Projection: gradient");
}
void GPUSolver::render() {
    // Download velocity data from GPU
    std::vector<float> data(gridWidth * gridHeight * 2);
    glBindTexture(GL_TEXTURE_2D, velocityTexture[currentBuffer]);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RG, GL_FLOAT, data.data());

    // Create visualization with enhanced colors
    std::vector<unsigned char> pixels(windowWidth * windowHeight * 3);

    // Find max velocity for normalization
    float maxVel = 0.0f;
    for (int i = 0; i < gridWidth * gridHeight * 2; i += 2) {
        float vx = data[i];
        float vy = data[i + 1];
        float mag = sqrt(vx*vx + vy*vy);
        maxVel = std::max(maxVel, mag);
    }

    // Ensure we have some normalization even for very small velocities
    if (maxVel < 0.01f) maxVel = 0.01f;

    for (int y = 0; y < windowHeight; y++) {
        for (int x = 0; x < windowWidth; x++) {
            int gx = (x * gridWidth) / windowWidth;
            int gy = (y * gridHeight) / windowHeight;

            // FIX: Access data correctly - texture data is stored row by row
            // The grid coordinate should map to the correct position in the 1D array
            float vx = data[(gy * gridWidth + gx) * 2];
            float vy = data[(gy * gridWidth + gx) * 2 + 1];
            float mag = sqrt(vx*vx + vy*vy);

            // Normalize and enhance visibility
            float normalized = std::min(mag / (maxVel * 0.3f), 1.0f);

            // Apply gamma correction for better visibility
            normalized = pow(normalized, 0.5f);

            // Create colorful visualization based on velocity direction and magnitude
            // FIX: Flip vertically - OpenGL textures start from bottom-left,
            // but we're rendering from top-left
            int flippedY = windowHeight - 1 - y;
            int idx = (flippedY * windowWidth + x) * 3;

            // Option 1: Heatmap (blue -> cyan -> green -> yellow -> red)
            if (normalized < 0.25f) {
                // Blue to Cyan
                float t = normalized / 0.25f;
                pixels[idx] = 0;
                pixels[idx+1] = static_cast<unsigned char>(t * 255);
                pixels[idx+2] = 255;
            } else if (normalized < 0.5f) {
                // Cyan to Green
                float t = (normalized - 0.25f) / 0.25f;
                pixels[idx] = 0;
                pixels[idx+1] = 255;
                pixels[idx+2] = static_cast<unsigned char>((1.0f - t) * 255);
            } else if (normalized < 0.75f) {
                // Green to Yellow
                float t = (normalized - 0.5f) / 0.25f;
                pixels[idx] = static_cast<unsigned char>(t * 255);
                pixels[idx+1] = 255;
                pixels[idx+2] = 0;
            } else {
                // Yellow to Red
                float t = (normalized - 0.75f) / 0.25f;
                pixels[idx] = 255;
                pixels[idx+1] = static_cast<unsigned char>((1.0f - t) * 255);
                pixels[idx+2] = 0;
            }
        }
    }

    // Update display texture
    glBindTexture(GL_TEXTURE_2D, displayTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, windowWidth, windowHeight, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    // Render to screen
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(displayShaderProgram);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, displayTexture);
    glUniform1i(glGetUniformLocation(displayShaderProgram, "displayTexture"), 0);

    glBindVertexArray(displayVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glfwSwapBuffers(window);
}
void GPUSolver::uploadVelocityData(const std::vector<std::vector<Vec>>& velocities) {
    std::vector<float> data(gridWidth * gridHeight * 2);

    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            int idx = (y * gridWidth + x) * 2;
            data[idx] = static_cast<float>(velocities[y][x].x);
            data[idx + 1] = static_cast<float>(velocities[y][x].y);
        }
    }

    glBindTexture(GL_TEXTURE_2D, velocityTexture[currentBuffer]);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, gridWidth, gridHeight, GL_RG, GL_FLOAT, data.data());
    checkGLError("uploadVelocityData");
}

void GPUSolver::downloadVelocityData(std::vector<std::vector<Vec>>& velocities) {
    std::vector<float> data(gridWidth * gridHeight * 2);

    glBindTexture(GL_TEXTURE_2D, velocityTexture[currentBuffer]);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RG, GL_FLOAT, data.data());
    checkGLError("downloadVelocityData");

    velocities.resize(gridHeight);
    for (int y = 0; y < gridHeight; y++) {
        velocities[y].resize(gridWidth);
        for (int x = 0; x < gridWidth; x++) {
            int idx = (y * gridWidth + x) * 2;
            velocities[y][x] = Vec(data[idx], data[idx + 1]);
        }
    }
}

void GPUSolver::addForce(int x, int y, float fx, float fy) {
    // Ensure coordinates are within grid bounds
    x = std::max(0, std::min(x, gridWidth - 1));
    y = std::max(0, std::min(y, gridHeight - 1));

    // Download current velocity field
    std::vector<std::vector<Vec>> velocities;
    downloadVelocityData(velocities);

    // Apply force in a small radius
    const int radius = 5;
    const float maxForce = 2.0f;  // Maximum force magnitude

    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            int px = x + dx;
            int py = y + dy;

            if (px >= 0 && px < gridWidth && py >= 0 && py < gridHeight) {
                float dist = std::sqrt(dx*dx + dy*dy);
                if (dist <= radius) {
                    float factor = (1.0f - dist/radius) * maxForce;
                    velocities[py][px].x += fx * factor;
                    velocities[py][px].y += fy * factor;
                }
            }
        }
    }

    // Upload modified velocity field
    uploadVelocityData(velocities);
}

void GPUSolver::addDye(int x, int y, float intensity) {
    // Optional: Add dye visualization
    // For now, just add some vertical force
    addForce(x, y, 0.0f, intensity * 0.1f);
}