#include "body.h"
#include <cmath>

Body::Body() 
    : id(0), mass(1.0), position(0, 0), velocity(0, 0), acceleration(0, 0), force(0, 0) {}

Body::Body(int id, double mass, const Vec2& position, const Vec2& velocity)
    : id(id), mass(mass), position(position), velocity(velocity), acceleration(0, 0), force(0, 0) {}

void Body::resetForce() {
    force = Vec2(0, 0);
}

void Body::updateAcceleration() {
    if (mass > 0) {
        acceleration = force / mass;
    }
}

void Body::updateVelocity(double dt) {
    velocity += acceleration * dt;
}

void Body::updatePosition(double dt) {
    position += velocity * dt;
}

double Body::getVisualRadius() const {
    return std::sqrt(mass);
}
