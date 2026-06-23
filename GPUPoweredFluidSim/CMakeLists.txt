cmake_minimum_required(VERSION 3.16)
project(NavierStokesSolver)

set(CMAKE_CXX_STANDARD 17)

# Include FetchContent module
include(FetchContent)

# Find OpenGL (system package)
find_package(OpenGL REQUIRED)

# Fetch and build GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
)

# Fetch and build GLEW (using glew-cmake since original GLEW doesn't have CMake)
FetchContent_Declare(
        glew
        GIT_REPOSITORY https://github.com/Perlmint/glew-cmake.git
        GIT_TAG glew-cmake-2.2.0
)

# Make dependencies available
FetchContent_MakeAvailable(glfw glew)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${OPENGL_INCLUDE_DIRS})

# Add executable for GPU solver (MAIN TARGET)
add_executable(NavierStokesSolverGPU
        main_gpu.cpp
        grid.cpp
        coords.cpp
        gpu_solver.cpp
        shader_manager.cpp
)

# Link libraries
target_link_libraries(NavierStokesSolverGPU
        ${OPENGL_LIBRARIES}
        glfw
        libglew_static
)

target_compile_features(NavierStokesSolverGPU PRIVATE cxx_std_17)

# Minimal compute test (secondary target)
add_executable(MinimalComputeTest
        minimal_compute_test.cpp
)

target_link_libraries(MinimalComputeTest
        ${OPENGL_LIBRARIES}
        glfw
        libglew_static
)

# Raylib-based visualizer (optional)
find_package(raylib QUIET)
if(raylib_FOUND)
    add_executable(NavierStokesVisualizer
            main_visualizer.cpp
            raylib_visualizer.cpp
            grid.cpp
            coords.cpp
    )
    target_link_libraries(NavierStokesVisualizer PRIVATE raylib)
    target_compile_features(NavierStokesVisualizer PRIVATE cxx_std_17)
else()
    message(STATUS "raylib not found; NavierStokesVisualizer will not be built.")
endif()