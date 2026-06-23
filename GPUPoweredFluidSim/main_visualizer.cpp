#include "raylib_visualizer.hpp"
#include <iostream>

int main(int argc, char** argv) {
    std::string filename = "finalframes.txt";
    if (argc > 1) filename = argv[1];

    RaylibVisualizer viz(800, 800);
    if (!viz.initialize()) return 1;

    if (!viz.loadFramesFromFile(filename)) {
        std::cerr << "Failed to load frames from " << filename << std::endl;
        return 1;
    }

    viz.run();
    viz.cleanup();
    return 0;
}
