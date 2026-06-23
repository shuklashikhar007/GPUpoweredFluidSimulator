#ifndef COORDS_HPP
#define COORDS_HPP

class Vec {
public:
    double x;
    double y;

    Vec(double x, double y) {
        this->x = x;
        this->y = y;
    }

    Vec() : x(0), y(0) {}

    static Vec add(Vec a, Vec b);
    static Vec sub(Vec a, Vec b);
    static Vec mult(Vec a, double scalar);
    static double dot(Vec a, Vec b);
    double magnitude();
};

#endif // COORDS_HPP