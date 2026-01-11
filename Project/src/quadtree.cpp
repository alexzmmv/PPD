#include "quadtree.h"
#include <algorithm>
#include <limits>
#include <cmath>

// ============================================================================
// AABB Implementation
// ============================================================================

AABB::AABB() : center(0, 0), halfSize(1.0) {}

AABB::AABB(const Vec2& center, double halfSize) : center(center), halfSize(halfSize) {}

bool AABB::contains(const Vec2& point) const {
    return (point.x >= center.x - halfSize && point.x <= center.x + halfSize &&
            point.y >= center.y - halfSize && point.y <= center.y + halfSize);
}

int AABB::getQuadrant(const Vec2& point) const {
    // 0 = NE (x >= center, y >= center)
    // 1 = NW (x < center, y >= center)
    // 2 = SW (x < center, y < center)
    // 3 = SE (x >= center, y < center)
    
    bool east = point.x >= center.x;
    bool north = point.y >= center.y;
    
    if (east && north) return 0;      // NE
    if (!east && north) return 1;     // NW
    if (!east && !north) return 2;    // SW
    return 3;                          // SE
}

AABB AABB::getChildAABB(int quadrant) const {
    double newHalfSize = halfSize / 2.0;
    Vec2 newCenter;
    
    switch (quadrant) {
        case 0: // NE
            newCenter = Vec2(center.x + newHalfSize, center.y + newHalfSize);
            break;
        case 1: // NW
            newCenter = Vec2(center.x - newHalfSize, center.y + newHalfSize);
            break;
        case 2: // SW
            newCenter = Vec2(center.x - newHalfSize, center.y - newHalfSize);
            break;
        case 3: // SE
            newCenter = Vec2(center.x + newHalfSize, center.y - newHalfSize);
            break;
    }
    
    return AABB(newCenter, newHalfSize);
}

// ============================================================================
// QuadTreeNode Implementation
// ============================================================================

QuadTreeNode::QuadTreeNode(const AABB& bounds) 
    : bounds(bounds), centerOfMass(0, 0), totalMass(0), body(nullptr), 
      isLeaf(true), isEmpty(true) {
    for (int i = 0; i < 4; i++) {
        children[i] = nullptr;
    }
}

void QuadTreeNode::subdivide() {
    for (int i = 0; i < 4; i++) {
        children[i] = std::make_unique<QuadTreeNode>(bounds.getChildAABB(i));
    }
    isLeaf = false;
}

bool QuadTreeNode::isExternal() const {
    return isLeaf && !isEmpty && body != nullptr;
}

void QuadTreeNode::insert(Body* newBody) {
    if (!bounds.contains(newBody->position)) {
        return; // Body is outside this node's bounds
    }
    
    if (isEmpty) {
        // First body in this node
        body = newBody;
        isEmpty = false;
        centerOfMass = newBody->position;
        totalMass = newBody->mass;
        return;
    }
    
    if (isLeaf) {
        // Need to subdivide and redistribute
        Body* existingBody = body;
        body = nullptr;
        subdivide();
        
        // Reinsert existing body
        int quadrant = bounds.getQuadrant(existingBody->position);
        children[quadrant]->insert(existingBody);
    }
    
    // Insert new body into appropriate child
    int quadrant = bounds.getQuadrant(newBody->position);
    children[quadrant]->insert(newBody);
    
    // Update center of mass and total mass
    double newTotalMass = totalMass + newBody->mass;
    centerOfMass = (centerOfMass * totalMass + newBody->position * newBody->mass) / newTotalMass;
    totalMass = newTotalMass;
}

void QuadTreeNode::calculateForce(Body* target, double theta, double G, double softening) const {
    if (isEmpty) {
        return;
    }
    
    // Don't calculate force on itself
    if (isExternal() && body == target) {
        return;
    }
    
    Vec2 diff = centerOfMass - target->position;
    double distSquared = diff.lengthSquared() + softening * softening;
    double dist = std::sqrt(distSquared);
    
    // Barnes-Hut criterion: s/d < theta (where s is the width of the region)
    double regionSize = bounds.halfSize * 2.0;
    
    if (isExternal() || (regionSize / dist < theta)) {
        // Treat this node as a single body (or it is a single body)
        // F = G * m1 * m2 / r^2 * r_hat
        // We accumulate force: F = G * m_target * m_node / r^2 * direction
        double forceMagnitude = G * target->mass * totalMass / distSquared;
        Vec2 forceDir = diff / dist;
        target->force += forceDir * forceMagnitude;
    } else {
        // Recurse into children
        for (int i = 0; i < 4; i++) {
            if (children[i]) {
                children[i]->calculateForce(target, theta, G, softening);
            }
        }
    }
}

// ============================================================================
// QuadTree Implementation
// ============================================================================

QuadTree::QuadTree() : root(nullptr) {}

void QuadTree::build(std::vector<Body>& bodies) {
    if (bodies.empty()) {
        root = nullptr;
        return;
    }
    
    AABB bounds = calculateBounds(bodies);
    root = std::make_unique<QuadTreeNode>(bounds);
    
    for (auto& body : bodies) {
        root->insert(&body);
    }
}

void QuadTree::calculateForces(std::vector<Body>& bodies, int startIdx, int endIdx,
                               double theta, double G, double softening) const {
    if (!root) return;
    
    // This function calculates forces for bodies[startIdx] to bodies[endIdx-1]
    // Designed to be easily parallelizable with threads or MPI
    for (int i = startIdx; i < endIdx; i++) {
        bodies[i].resetForce();
        root->calculateForce(&bodies[i], theta, G, softening);
    }
}

void QuadTree::clear() {
    root.reset();
}

AABB QuadTree::calculateBounds(const std::vector<Body>& bodies) const {
    if (bodies.empty()) {
        return AABB(Vec2(0, 0), 1.0);
    }
    
    double minX = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest();
    double minY = std::numeric_limits<double>::max();
    double maxY = std::numeric_limits<double>::lowest();
    
    for (const auto& body : bodies) {
        minX = std::min(minX, body.position.x);
        maxX = std::max(maxX, body.position.x);
        minY = std::min(minY, body.position.y);
        maxY = std::max(maxY, body.position.y);
    }
    
    // Add some padding
    double padding = 10.0;
    minX -= padding;
    maxX += padding;
    minY -= padding;
    maxY += padding;
    
    Vec2 center((minX + maxX) / 2.0, (minY + maxY) / 2.0);
    double halfSize = std::max(maxX - minX, maxY - minY) / 2.0;
    
    return AABB(center, halfSize);
}
