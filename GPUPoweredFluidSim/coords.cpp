#include "coords.hpp"
#include <cmath>

Vec Vec::add(Vec a, Vec b) {
    Vec res;
    res.x = a.x + b.x;
    res.y = a.y + b.y;
    return res;
}

Vec Vec::sub(Vec a, Vec b) {
    Vec res;
    res.x = a.x - b.x;
    res.y = a.y - b.y;
    return res;
}

Vec Vec::mult(Vec a, double scalar) {
    Vec res;
    res.x = a.x * scalar;
    res.y = a.y * scalar;
    return res;
}

double Vec::dot(Vec a, Vec b) {
    return a.x * b.x + a.y * b.y;
}

double Vec::magnitude() {
    return sqrt(pow(this->x, 2) + pow(this->y, 2));
}