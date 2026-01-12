// File: Project/include/visualizer.h
#ifndef VISUALIZER_H
#define VISUALIZER_H

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <string>
#include <fstream>
#include <map>
#include "body.h"
#include "vec2.h"

struct BodyState {
    int id;
    Vec2 position;
    double radius;
};

struct SimulationFrame {
    int stepNumber;
    std::vector<BodyState> bodies;
};

class Visualizer {
private:
    GLFWwindow* window;
    int windowWidth;
    int windowHeight;
    
    // Simulation data
    std::vector<SimulationFrame> threadedFrames;
    std::vector<SimulationFrame> mpiFrames;
    
    // Animation control
    int currentFrame;
    int maxFrames;
    bool isPlaying;
    bool showBothSims;
    double animationSpeed;
    double lastFrameTime;
    
    // View parameters
    double viewScale;
    Vec2 viewCenter;
    double minX, maxX, minY, maxY; // World bounds
    
    // Colors for different body masses
    struct Color {
        float r, g, b, a;
        Color(float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f) 
            : r(r), g(g), b(b), a(a) {}
    };
    
    std::map<int, Color> bodyColors;
    
public:
    Visualizer(int width = 1600, int height = 800);
    ~Visualizer();
    
    // Initialization
    bool initialize();
    void setupColors();
    
    // Data loading
    bool loadSimulationData(const std::string& threadedFile, const std::string& mpiFile);
    bool parseOutputFile(const std::string& filename, std::vector<SimulationFrame>& frames);
    
    // Rendering
    void render();
    void renderSimulation(const std::vector<SimulationFrame>& frames, float offsetX, float width);
    void renderBodies(const std::vector<BodyState>& bodies, float offsetX, float width);
    void renderUI();
    
    // Utility functions
    void drawCircle(float x, float y, float radius, const Color& color);
    void drawText(float x, float y, const std::string& text);
    Vec2 worldToScreen(const Vec2& worldPos, float offsetX, float width);
    void calculateWorldBounds();
    void updateAnimation();
    
    // Input handling
    void handleInput();
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    
    // Main loop
    void run();
    bool shouldClose() const;
    void swapBuffers();
    void pollEvents();
};

#endif // VISUALIZER_H
