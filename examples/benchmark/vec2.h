#pragma once

struct vec2f
{
    float x = 0;
    float y = 0;

    vec2f() = default;
    vec2f(float x, float y) : x(x), y(y) {}

    inline vec2f operator+(const vec2f& v) const
    {
        return {x + v.x, y + v.y};
    }
    inline vec2f operator+(float s) const
    {
        return {x + s, y + s};
    }
    inline vec2f& operator+=(const vec2f& v)
    {
        x += v.x;
        y += v.y;
        return *this;
    }
    inline vec2f& operator+=(float s)
    {
        x += s;
        y += s;
        return *this;
    }
    inline vec2f operator-(const vec2f& v) const
    {
        return {x - v.x, y - v.y};
    }
    inline vec2f operator-(float s) const
    {
        return {x - s, y - s};
    }
    inline vec2f& operator-=(const vec2f& v)
    {
        x -= v.x;
        y -= v.y;
        return *this;
    }
    inline vec2f& operator-=(float s)
    {
        x -= s;
        y -= s;
        return *this;
    }
    inline vec2f operator-() const
    {
        return {-x, -y};
    }
    inline vec2f operator*(const vec2f& v) const
    {
        return {x * v.x, y * v.y};
    }
    inline vec2f operator*(float s) const
    {
        return {x * s, y * s};
    }
    inline vec2f& operator*=(const vec2f& v)
    {
        x *= v.x;
        y *= v.y;
        return *this;
    }
    inline vec2f& operator*=(float s)
    {
        x *= s;
        y *= s;
        return *this;
    }
    inline vec2f operator/(const vec2f& v) const
    {
        return {x / v.x, y / v.y};
    }
    inline vec2f operator/(float s) const
    {
        return {x / s, y / s};
    }
    inline vec2f& operator/=(const vec2f& v)
    {
        x /= v.x;
        y /= v.y;
        return *this;
    }
    inline vec2f& operator/=(float s)
    {
        x /= s;
        y /= s;
        return *this;
    }
};

inline vec2f operator+(float s, const vec2f& v)
{
    return {s + v.x, s + v.y};
}
inline vec2f operator-(float s, const vec2f& v)
{
    return {s - v.x, s - v.y};
}
inline vec2f operator*(float s, const vec2f& v)
{
    return {s * v.x, s * v.y};
}
inline vec2f operator/(float s, const vec2f& v)
{
    return {s / v.x, s / v.y};
}

inline vec2f abs(const vec2f& v)
{
    return {fabsf(v.x), fabsf(v.y)};
}
inline float length_sq(const vec2f& v)
{
    return v.x*v.x + v.y*v.y;
}
inline float length(const vec2f& v)
{
    return std::sqrt(v.x*v.x + v.y*v.y);
}
inline float distance_sq(const vec2f& v1, const vec2f& v2)
{
    vec2f d = v1 - v2;
    return d.x*d.x + d.y*d.y;
}
inline float distance(const vec2f& v1, const vec2f& v2)
{
    vec2f d = v1 - v2;
    return std::sqrt(d.x*d.x + d.y*d.y);
}
inline void normalize(vec2f& v)
{
    float l = length(v);
    v /= l;
}
inline vec2f normalized(const vec2f& v)
{
    float l = length(v);
    return v / l;
}
inline float dot(const vec2f& v1, const vec2f& v2)
{
    return v1.x*v2.x + v1.y*v2.y;
}