#pragma once

#include <stdint.h>

class Touchscreen
{
public:
    virtual ~Touchscreen() = default;

    class Point
    {
    public:
        Point(void) : x(0), y(0), z(0) {}
        Point(int16_t x, int16_t y, int16_t z) : x(x), y(y), z(z) {}
        bool operator==(Point p) { return ((p.x == x) && (p.y == y) && (p.z == z)); }
        bool operator!=(Point p) { return ((p.x != x) || (p.y != y) || (p.z != z)); }
        int16_t x, y, z;
    };

	virtual Point getPoint() = 0;
	virtual bool touched() = 0;
	virtual void setRotation(uint8_t n) = 0;
};
