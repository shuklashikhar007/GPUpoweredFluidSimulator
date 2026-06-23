#pragma once

#include <GL/glew.h>
#include <string>
#include <unordered_map>
#include <iostream>

class ShaderManager {
private:
    // Store shader and program handles
    std::unordered_map<std::string, GLuint> shaders;
    std::unordered_map<std::string, GLuint> programs;

public:
    ShaderManager() = default;
    ~ShaderManager() { cleanup(); }

    // Core shader management functions
    GLuint createComputeShader(const std::string& name, const std::string& source);
    GLuint createComputeProgram(const std::string& name, GLuint shader);

    // Getters for shader and program handles
    GLuint getProgram(const std::string& name) const {
        auto it = programs.find(name);
        return (it != programs.end()) ? it->second : 0;
    }

    GLuint getShader(const std::string& name) const {
        auto it = shaders.find(name);
        return (it != shaders.end()) ? it->second : 0;
    }

    // Cleanup resources
    void cleanup() {
        for (const auto& [name, program] : programs) {
            if (program) {
                glDeleteProgram(program);
            }
        }
        programs.clear();

        for (const auto& [name, shader] : shaders) {
            if (shader) {
                glDeleteShader(shader);
            }
        }
        shaders.clear();
    }

    // Shader source code definitions
    static const std::string FORCE_SHADER_SOURCE;
    static const std::string DIFFUSION_SHADER_SOURCE;
    static const std::string ADVECTION_SHADER_SOURCE;
    static const std::string PROJECTION_SHADER_SOURCE;
    static const std::string PROJECTION_GRADIENT_SHADER_SOURCE;
    static const std::string BOUNDARY_SHADER_SOURCE;
    static const std::string VISUALIZATION_SHADER_SOURCE;
};