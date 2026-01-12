// File: Project/src/vec2.cpp
#include "vec2.h"
#include <cmath>
#include <iostream>

Vec2::Vec2() : x(0.0), y(0.0) {}

Vec2::Vec2(double x, double y) : x(x), y(y) {}

Vec2 Vec2::operator+(const Vec2& other) const {
    return Vec2(x + other.x, y + other.y);
}

Vec2 Vec2::operator-(const Vec2& other) const {
    return Vec2(x - other.x, y - other.y);
}

Vec2 Vec2::operator*(double scalar) const {
    return Vec2(x * scalar, y * scalar);
}

Vec2 Vec2::operator/(double scalar) const {
    return Vec2(x / scalar, y / scalar);
}

Vec2& Vec2::operator+=(const Vec2& other) {
    x += other.x;
    y += other.y;
    return *this;
}

Vec2& Vec2::operator-=(const Vec2& other) {
    x -= other.x;
    y -= other.y;
    return *this;
}

Vec2& Vec2::operator*=(double scalar) {
    x *= scalar;
    y *= scalar;
    return *this;
}

Vec2& Vec2::operator/=(double scalar) {
    x /= scalar;
    y /= scalar;
    return *this;
}

double Vec2::dot(const Vec2& other) const {
    return x * other.x + y * other.y;
}

double Vec2::lengthSquared() const {
    return x * x + y * y;
}

double Vec2::length() const {
    return std::sqrt(lengthSquared());
}

Vec2 Vec2::normalized() const {
    double len = length();
    if (len > 0.0) {
        return *this / len;
    }
    return Vec2(0.0, 0.0);
}

double Vec2::distance(const Vec2& a, const Vec2& b) {
    return (a - b).length();
}

double Vec2::distanceSquared(const Vec2& a, const Vec2& b) {
    return (a - b).lengthSquared();
}

std::ostream& operator<<(std::ostream& os, const Vec2& v) {
    os << "(" << v.x << ", " << v.y << ")";
    return os;
}
