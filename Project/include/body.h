#ifndef BODY_H
#define BODY_H

#include "vec2.h"

class Body {
public:
    int id;
    double mass;
    Vec2 position;
    Vec2 velocity;
    Vec2 acceleration;
    Vec2 force;

    Body();
    Body(int id, double mass, const Vec2& position, const Vec2& velocity);

    // Reset force accumulator for new calculation step
    void resetForce();

    // Apply accumulated force to update acceleration
    void updateAcceleration();

    // Update velocity using current acceleration (leapfrog integration)
    void updateVelocity(double dt);

    // Update position using current velocity (leapfrog integration)
    void updatePosition(double dt);

    // Get visual radius (square root of mass)
    double getVisualRadius() const;
};

#endif // BODY_H
