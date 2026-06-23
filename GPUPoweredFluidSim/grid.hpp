#ifndef GRID_HPP
#define GRID_HPP

#include "coords.hpp"
#include <vector>
#include <string>
#include <fstream>
extern const int width;
extern const int height;
extern const double kinematicViscosity;
extern const int diffusionIterations;
extern const int projectionIterations;
extern const int dx;
using namespace std;

class grid {
public:
    vector <vector <Vec>> currentVelocities;
    vector <vector <Vec>> nextVelocities;
    vector <vector <double>> pressureForces;
    double timeStep;
    double alpha;

    vector <vector<vector<Vec>>> frames;
    vector <vector<vector<Vec>>> generatedFrames;

    //core logic
    void forces();
    void diffusion();
    void projection();
    void advection();
    void frameGen();

    //file io
    void writeFramesToFile(const string& filename);
    void readFramesFromFile(const string& filename);

    //helper functions
    void renderNext();
    void init();
    Vec getBoundaryVelocity(int i, int j, const vector<vector<Vec>>& velocities);
    double getBoundaryPressure(int i, int j, const vector<vector<double>> pressureForces);
};

#endif // GRID_HPP