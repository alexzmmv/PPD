#ifndef QUADTREE_H
#define QUADTREE_H

#include "vec2.h"
#include "body.h"
#include <vector>
#include <memory>

// Axis-Aligned Bounding Box for quadtree regions
class AABB {
public:
    Vec2 center;
    double halfSize;

    AABB();
    AABB(const Vec2& center, double halfSize);

    bool contains(const Vec2& point) const;
    
    // Get quadrant index (0=NE, 1=NW, 2=SW, 3=SE)
    int getQuadrant(const Vec2& point) const;
    
    // Create child AABB for given quadrant
    AABB getChildAABB(int quadrant) const;
};

// QuadTree node for Barnes-Hut algorithm
class QuadTreeNode {
public:
    AABB bounds;
    
    // Center of mass and total mass for this node
    Vec2 centerOfMass;
    double totalMass;
    
    // If this is a leaf with a single body
    Body* body;
    bool isLeaf;
    bool isEmpty;
    
    // Children: NE, NW, SW, SE
    std::unique_ptr<QuadTreeNode> children[4];

    QuadTreeNode(const AABB& bounds);

    // Insert a body into the tree
    void insert(Body* body);

    // Calculate force on a body using Barnes-Hut approximation
    // theta: opening angle threshold (typically 0.5)
    // G: gravitational constant
    // softening: softening parameter to avoid singularities
    void calculateForce(Body* target, double theta, double G, double softening) const;

private:
    void subdivide();
    bool isExternal() const;
};

// QuadTree manager class - builds and manages the tree
class QuadTree {
public:
    std::unique_ptr<QuadTreeNode> root;
    
    QuadTree();

    // Build tree from a vector of bodies
    void build(std::vector<Body>& bodies);

    // Calculate forces on all bodies in a range (for parallel processing)
    // This is designed to be easily adaptable for MPI
    void calculateForces(std::vector<Body>& bodies, int startIdx, int endIdx, 
                         double theta, double G, double softening) const;

    // Clear the tree
    void clear();

private:
    // Calculate bounding box that contains all bodies
    AABB calculateBounds(const std::vector<Body>& bodies) const;
};

#endif // QUADTREE_H
