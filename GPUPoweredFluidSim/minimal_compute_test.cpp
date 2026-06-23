#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>

// MINIMAL COMPUTE SHADER TEST - Does it even work?

int main() {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);  // Hidden window

    GLFWwindow* window = glfwCreateWindow(640, 480, "Test", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create window" << std::endl;
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);

    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return 1;
    }

    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GPU: " << glGetString(GL_RENDERER) << std::endl;

    if (!GLEW_ARB_compute_shader) {
        std::cerr << "Compute shaders NOT supported!" << std::endl;
        return 1;
    }
    std::cout << "Compute shaders supported!" << std::endl;

    // Create a simple texture (256x256, RG32F)
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, 256, 256, 0, GL_RG, GL_FLOAT, nullptr);

    std::cout << "Created texture" << std::endl;

    // Clear it to zeros
    std::vector<float> zeros(256 * 256 * 2, 0.0f);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 256, GL_RG, GL_FLOAT, zeros.data());

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "OpenGL error after texture creation: " << err << std::endl;
    }

    // Create compute shader
    const char* shaderSource = R"(
        #version 430 core
        layout(local_size_x = 16, local_size_y = 16) in;
        layout(rg32f, binding = 0) uniform image2D img;

        void main() {
            ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
            if (coord.x >= 256 || coord.y >= 256) return;

            // Set all pixels to (5.0, 3.0)
            imageStore(img, coord, vec4(5.0, 3.0, 0.0, 1.0));
        }
    )";

    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(shader, 1, &shaderSource, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar infoLog[1024];
        glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
        std::cerr << "Shader compilation failed: " << infoLog << std::endl;
        return 1;
    }
    std::cout << "Shader compiled successfully!" << std::endl;

    GLuint program = glCreateProgram();
    glAttachShader(program, shader);
    glLinkProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLchar infoLog[1024];
        glGetProgramInfoLog(program, 1024, nullptr, infoLog);
        std::cerr << "Program linking failed: " << infoLog << std::endl;
        return 1;
    }
    std::cout << "Program linked successfully!" << std::endl;

    // Use program
    glUseProgram(program);

    err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "OpenGL error after glUseProgram: " << err << std::endl;
    }

    // Bind texture as image
    glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RG32F);

    err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "OpenGL error after glBindImageTexture: " << err << std::endl;
    }

    std::cout << "Dispatching compute shader..." << std::endl;

    // Dispatch
    glDispatchCompute(16, 16, 1);  // 256/16 = 16

    err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "OpenGL error after glDispatchCompute: " << err << std::endl;
    }

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "OpenGL error after glMemoryBarrier: " << err << std::endl;
    }

    std::cout << "Compute shader dispatched!" << std::endl;

    // Read back data
    std::vector<float> data(256 * 256 * 2);
    glBindTexture(GL_TEXTURE_2D, texture);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RG, GL_FLOAT, data.data());

    err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "OpenGL error after glGetTexImage: " << err << std::endl;
    }

    // Check results
    std::cout << "\nResults:" << std::endl;
    std::cout << "First pixel: (" << data[0] << ", " << data[1] << ")" << std::endl;
    std::cout << "Pixel [100,100]: (" << data[(100*256 + 100)*2] << ", " << data[(100*256 + 100)*2 + 1] << ")" << std::endl;

    float maxVal = 0.0f;
    for (size_t i = 0; i < data.size(); i++) {
        maxVal = std::max(maxVal, std::abs(data[i]));
    }
    std::cout << "Max value in texture: " << maxVal << std::endl;

    if (maxVal > 0.1f) {
        std::cout << "\n✅ SUCCESS! Compute shader worked!" << std::endl;
    } else {
        std::cout << "\n❌ FAILED! Compute shader did not modify texture!" << std::endl;
    }

    // Cleanup
    glDeleteProgram(program);
    glDeleteShader(shader);
    glDeleteTextures(1, &texture);
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}