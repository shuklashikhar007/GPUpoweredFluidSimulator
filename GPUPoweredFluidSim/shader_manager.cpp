#include "shader_manager.hpp"
#include <iostream>

GLuint ShaderManager::createComputeShader(const std::string& name, const std::string& source) {
    std::cout << "\nCreating shader: " << name << std::endl;

    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
    if (shader == 0) {
        std::cerr << "Failed to create shader object" << std::endl;
        return 0;
    }

    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar infoLog[1024];
        glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
        std::cerr << "Shader compilation failed:\n" << infoLog << std::endl;
        glDeleteShader(shader);
        return 0;
    }

    shaders[name] = shader;
    std::cout << "Shader compiled successfully: " << name << " (ID: " << shader << ")" << std::endl;
    return shader;
}

GLuint ShaderManager::createComputeProgram(const std::string& name, GLuint shader) {
    std::cout << "\nCreating program: " << name << std::endl;

    if (shader == 0) {
        std::cerr << "Invalid shader ID" << std::endl;
        return 0;
    }

    GLuint program = glCreateProgram();
    if (program == 0) {
        std::cerr << "Failed to create program" << std::endl;
        return 0;
    }

    glAttachShader(program, shader);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLchar infoLog[1024];
        glGetProgramInfoLog(program, 1024, nullptr, infoLog);
        std::cerr << "Program linking failed:\n" << infoLog << std::endl;
        glDeleteProgram(program);
        return 0;
    }

    programs[name] = program;
    std::cout << "Program created successfully: " << name << " (ID: " << program << ")" << std::endl;
    return program;
}

// Shader Sources
const std::string ShaderManager::FORCE_SHADER_SOURCE = R"(
#version 430
layout(local_size_x = 16, local_size_y = 16) in;
layout(rg32f, binding = 0) uniform image2D velocityField;

uniform float timeStep;
uniform int width;
uniform int height;

void main() {

}
)";

const std::string ShaderManager::DIFFUSION_SHADER_SOURCE = R"(
#version 430
layout(local_size_x = 16, local_size_y = 16) in;
layout(rg32f, binding = 0) uniform image2D velocityOut;
layout(rg32f, binding = 1) uniform image2D velocityIn;
layout(rg32f, binding = 2) uniform image2D velocityBefore;

uniform int width;
uniform int height;
uniform float alpha;

void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    if (pos.x >= width || pos.y >= height) return;

    // Get neighbor positions (clamped to boundaries)
    ivec2 left = ivec2(max(pos.x - 1, 0), pos.y);
    ivec2 right = ivec2(min(pos.x + 1, width-1), pos.y);
    ivec2 up = ivec2(pos.x, max(pos.y - 1, 0));
    ivec2 down = ivec2(pos.x, min(pos.y + 1, height-1));

    // Sample velocities
    vec2 vL = imageLoad(velocityIn, left).xy;
    vec2 vR = imageLoad(velocityIn, right).xy;
    vec2 vU = imageLoad(velocityIn, up).xy;
    vec2 vD = imageLoad(velocityIn, down).xy;
    vec2 vC = imageLoad(velocityBefore, pos).xy;

    // Jacobi iteration for diffusion
    vec2 result = (vC + alpha * (vL + vR + vU + vD)) / (1.0 + 4.0 * alpha);

    imageStore(velocityOut, pos, vec4(result, 0.0, 1.0));
}
)";

const std::string ShaderManager::ADVECTION_SHADER_SOURCE = R"(
#version 430
layout(local_size_x = 16, local_size_y = 16) in;
layout(rg32f, binding = 0) uniform image2D velocityOut;
layout(rg32f, binding = 1) uniform image2D velocityIn;

uniform int width;
uniform int height;
uniform float timeStep;

void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    if (pos.x >= width || pos.y >= height) return;

    // Get current velocity
    vec2 vel = imageLoad(velocityIn, pos).xy;

    // Backtrace to find where this particle came from
    vec2 prevPos = vec2(pos) - vel * timeStep;
    prevPos = clamp(prevPos, vec2(0), vec2(width-1, height-1));

    // Bilinear interpolation
    ivec2 i0 = ivec2(floor(prevPos));
    ivec2 i1 = min(i0 + 1, ivec2(width-1, height-1));
    vec2 f = fract(prevPos);

    vec2 v00 = imageLoad(velocityIn, i0).xy;
    vec2 v10 = imageLoad(velocityIn, ivec2(i1.x, i0.y)).xy;
    vec2 v01 = imageLoad(velocityIn, ivec2(i0.x, i1.y)).xy;
    vec2 v11 = imageLoad(velocityIn, i1).xy;

    vec2 result = mix(
        mix(v00, v10, f.x),
        mix(v01, v11, f.x),
        f.y
    );

    imageStore(velocityOut, pos, vec4(result, 0.0, 1.0));
}
)";

const std::string ShaderManager::PROJECTION_SHADER_SOURCE = R"(
#version 430
layout(local_size_x = 16, local_size_y = 16) in;
layout(rg32f, binding = 0) uniform image2D velocityField;
layout(r32f, binding = 1) uniform image2D pressureOut;
layout(r32f, binding = 2) uniform image2D pressureIn;
layout(r32f, binding = 3) uniform image2D divergenceField;

uniform int width;
uniform int height;
uniform int mode;  // 0=divergence, 1=red, 2=black

void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    if (pos.x >= width || pos.y >= height) return;

    if (mode == 0) {
        // Compute divergence of velocity field
        ivec2 left = ivec2(max(pos.x - 1, 0), pos.y);
        ivec2 right = ivec2(min(pos.x + 1, width-1), pos.y);
        ivec2 up = ivec2(pos.x, max(pos.y - 1, 0));
        ivec2 down = ivec2(pos.x, min(pos.y + 1, height-1));

        vec2 vL = imageLoad(velocityField, left).xy;
        vec2 vR = imageLoad(velocityField, right).xy;
        vec2 vU = imageLoad(velocityField, up).xy;
        vec2 vD = imageLoad(velocityField, down).xy;

        float div = -0.5 * ((vR.x - vL.x) + (vD.y - vU.y));
        imageStore(divergenceField, pos, vec4(div, 0.0, 0.0, 1.0));
    }
    else {
        // Gauss-Seidel red-black iteration for pressure solve
        bool isRed = ((pos.x + pos.y) % 2) == 0;

        // Only update appropriate cells based on mode
        if ((mode == 1) != isRed) {
            // Just copy the existing pressure value
            float p = imageLoad(pressureIn, pos).x;
            imageStore(pressureOut, pos, vec4(p, 0.0, 0.0, 1.0));
            return;
        }

        ivec2 left = ivec2(max(pos.x - 1, 0), pos.y);
        ivec2 right = ivec2(min(pos.x + 1, width-1), pos.y);
        ivec2 up = ivec2(pos.x, max(pos.y - 1, 0));
        ivec2 down = ivec2(pos.x, min(pos.y + 1, height-1));

        float pL = imageLoad(pressureIn, left).x;
        float pR = imageLoad(pressureIn, right).x;
        float pU = imageLoad(pressureIn, up).x;
        float pD = imageLoad(pressureIn, down).x;
        float div = imageLoad(divergenceField, pos).x;

        // Jacobi iteration for Poisson equation
        float p = (div + pL + pR + pU + pD) / 4.0;
        imageStore(pressureOut, pos, vec4(p, 0.0, 0.0, 1.0));
    }
}
)";

const std::string ShaderManager::PROJECTION_GRADIENT_SHADER_SOURCE = R"(
#version 430
layout(local_size_x = 16, local_size_y = 16) in;
layout(rg32f, binding = 0) uniform image2D velocityField;
layout(r32f, binding = 1) uniform image2D pressureField;

uniform int width;
uniform int height;

void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    if (pos.x >= width || pos.y >= height) return;

    // Get neighbor positions
    ivec2 left = ivec2(max(pos.x - 1, 0), pos.y);
    ivec2 right = ivec2(min(pos.x + 1, width-1), pos.y);
    ivec2 up = ivec2(pos.x, max(pos.y - 1, 0));
    ivec2 down = ivec2(pos.x, min(pos.y + 1, height-1));

    // Sample pressure values
    float pL = imageLoad(pressureField, left).x;
    float pR = imageLoad(pressureField, right).x;
    float pU = imageLoad(pressureField, up).x;
    float pD = imageLoad(pressureField, down).x;

    // Compute pressure gradient
    vec2 gradient = vec2(pR - pL, pD - pU) * 0.5;

    // Subtract gradient from velocity to make it divergence-free
    vec2 velocity = imageLoad(velocityField, pos).xy - gradient;

    imageStore(velocityField, pos, vec4(velocity, 0.0, 1.0));
}
)";

const std::string ShaderManager::BOUNDARY_SHADER_SOURCE = R"(
#version 430
layout(local_size_x = 16, local_size_y = 16) in;
layout(rg32f, binding = 0) uniform image2D velocityField;

uniform int width;
uniform int height;

void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    if (pos.x >= width || pos.y >= height) return;

    // Apply boundary conditions (zero velocity at boundaries)
    if (pos.x == 0 || pos.x == width-1 || pos.y == 0 || pos.y == height-1) {
        imageStore(velocityField, pos, vec4(0.0));
    }
}
)";

const std::string ShaderManager::VISUALIZATION_SHADER_SOURCE = R"(
#version 430
layout(local_size_x = 16, local_size_y = 16) in;
layout(rg32f, binding = 0) uniform image2D velocityField;
layout(rgba8, binding = 1) uniform image2D outputImage;

uniform int width;
uniform int height;

void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    if (pos.x >= width || pos.y >= height) return;

    vec2 velocity = imageLoad(velocityField, pos).xy;
    float magnitude = length(velocity);

    // Map velocity magnitude to color
    vec3 color = vec3(magnitude / 5.0);

    imageStore(outputImage, pos, vec4(color, 1.0));
}
)";