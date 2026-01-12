// File: Project/src/visualizer.cpp
#include "visualizer.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <iomanip>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Visualizer::Visualizer(int width, int height)
    : window(nullptr), windowWidth(width), windowHeight(height),
    currentFrame(0), maxFrames(0), isPlaying(true), showBothSims(true),
    animationSpeed(1.0), lastFrameTime(0.0), viewScale(1.0), viewCenter(0, 0),
    minX(-300), maxX(300), minY(-300), maxY(300) {
    setupColors();
}

Visualizer::~Visualizer() {
    if (window) {
        glfwDestroyWindow(window);
        glfwTerminate();
    }
}

bool Visualizer::initialize() {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }

    // Configure GLFW - Use compatibility profile for legacy OpenGL
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE); // Allow any profile
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    // Create window
    window = glfwCreateWindow(windowWidth, windowHeight, "N-Body Simulation Visualization", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

    // Make context current
    glfwMakeContextCurrent(window);

    // Set callbacks
    glfwSetWindowUserPointer(window, this);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetScrollCallback(window, scrollCallback);

    // Initialize GLEW
    glewExperimental = GL_TRUE; // Enable experimental features
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return false;
    }

    // Clear any OpenGL errors from GLEW initialization
    glGetError();

    // OpenGL settings
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.1f, 0.1f, 0.2f, 1.0f); // Dark blue background

    // Enable VSync
    glfwSwapInterval(1);

    std::cout << "OpenGL Renderer: " << glGetString(GL_RENDERER) << std::endl;
    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;

    return true;
}

void Visualizer::setupColors() {
    // Setup colors based on body mass/id
    bodyColors[1] = Color(1.0f, 1.0f, 0.0f, 1.0f);  // Yellow - largest body
    bodyColors[2] = Color(0.0f, 1.0f, 1.0f, 1.0f);  // Cyan
    bodyColors[3] = Color(1.0f, 0.0f, 1.0f, 1.0f);  // Magenta
    bodyColors[4] = Color(0.0f, 1.0f, 0.0f, 1.0f);  // Green
    bodyColors[5] = Color(1.0f, 0.5f, 0.0f, 1.0f);  // Orange
    bodyColors[6] = Color(0.5f, 0.0f, 1.0f, 1.0f);  // Purple
    bodyColors[7] = Color(1.0f, 0.0f, 0.0f, 1.0f);  // Red
    bodyColors[8] = Color(0.0f, 0.5f, 1.0f, 1.0f);  // Light Blue
    bodyColors[9] = Color(1.0f, 1.0f, 1.0f, 1.0f);  // White
    bodyColors[10] = Color(0.7f, 0.7f, 0.7f, 1.0f); // Gray
}

bool Visualizer::loadSimulationData(const std::string& threadedFile, const std::string& mpiFile) {
    std::cout << "Loading simulation data..." << std::endl;

    bool threadedLoaded = parseOutputFile(threadedFile, threadedFrames);
    bool mpiLoaded = parseOutputFile(mpiFile, mpiFrames);

    if (!threadedLoaded && !mpiLoaded) {
        std::cerr << "Failed to load simulation data from both files" << std::endl;
        return false;
    }

    if (!threadedLoaded) {
        std::cout << "Warning: Could not load threaded simulation data from: " << threadedFile << std::endl;
        showBothSims = false;
    }

    if (!mpiLoaded) {
        std::cout << "Warning: Could not load MPI simulation data from: " << mpiFile << std::endl;
        showBothSims = false;
    }

    maxFrames = std::max(threadedFrames.size(), mpiFrames.size());

    std::cout << "Loaded " << threadedFrames.size() << " threaded frames" << std::endl;
    std::cout << "Loaded " << mpiFrames.size() << " MPI frames" << std::endl;
    std::cout << "Total frames: " << maxFrames << std::endl;

    if (maxFrames == 0) {
        std::cerr << "No simulation data loaded!" << std::endl;
        return false;
    }

    calculateWorldBounds();

    return true;
}

bool Visualizer::parseOutputFile(const std::string& filename, std::vector<SimulationFrame>& frames) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Could not open file: " << filename << std::endl;
        return false;
    }

    frames.clear();
    std::string line;
    SimulationFrame currentFrame;
    bool inFrame = false;
    int lineCount = 0;

    std::cout << "Parsing file: " << filename << std::endl;

    while (std::getline(file, line)) {
        lineCount++;

        // Skip empty lines
        if (line.empty()) {
            if (inFrame && !currentFrame.bodies.empty()) {
                frames.push_back(currentFrame);
                inFrame = false;
            }
            continue;
        }

        // Check for step header
        if (line.substr(0, 4) == "step") {
            if (inFrame && !currentFrame.bodies.empty()) {
                frames.push_back(currentFrame);
            }

            std::istringstream iss(line);
            std::string stepWord;
            iss >> stepWord >> currentFrame.stepNumber;
            currentFrame.bodies.clear();
            inFrame = true;

            if (frames.size() % 50 == 0 && frames.size() > 0) {
                std::cout << "  Loaded " << frames.size() << " frames..." << std::endl;
            }
        }
        else if (inFrame) {
            // Parse body data: id x y
            std::istringstream iss(line);
            BodyState body;
            if (iss >> body.id >> body.position.x >> body.position.y) {
                // Calculate radius based on body ID (adjust based on your simulation)
                if (body.id == 1) {
                    body.radius = 15.0; // Largest body
                }
                else if (body.id <= 5) {
                    body.radius = 8.0;  // Medium bodies
                }
                else {
                    body.radius = 5.0;  // Small bodies
                }
                currentFrame.bodies.push_back(body);
            }
        }
    }

    // Don't forget the last frame
    if (inFrame && !currentFrame.bodies.empty()) {
        frames.push_back(currentFrame);
    }

    file.close();

    std::cout << "  Parsed " << lineCount << " lines, loaded " << frames.size() << " frames from " << filename << std::endl;

    return !frames.empty();
}

void Visualizer::calculateWorldBounds() {
    if (threadedFrames.empty() && mpiFrames.empty()) {
        std::cout << "No frames to calculate bounds from" << std::endl;
        return;
    }

    minX = minY = 1e9;
    maxX = maxY = -1e9;

    auto updateBounds = [&](const std::vector<SimulationFrame>& frames) {
        for (const auto& frame : frames) {
            for (const auto& body : frame.bodies) {
                minX = std::min(minX, body.position.x - body.radius);
                maxX = std::max(maxX, body.position.x + body.radius);
                minY = std::min(minY, body.position.y - body.radius);
                maxY = std::max(maxY, body.position.y + body.radius);
            }
        }
        };

    updateBounds(threadedFrames);
    updateBounds(mpiFrames);

    // Add some padding
    double paddingX = (maxX - minX) * 0.1;
    double paddingY = (maxY - minY) * 0.1;
    minX -= paddingX;
    maxX += paddingX;
    minY -= paddingY;
    maxY += paddingY;

    std::cout << "World bounds: (" << minX << ", " << minY << ") to (" << maxX << ", " << maxY << ")" << std::endl;
}

void Visualizer::render() {
    // Clear the screen
    glClear(GL_COLOR_BUFFER_BIT);

    // Set viewport
    glViewport(0, 0, windowWidth, windowHeight);

    // Set up 2D orthographic projection
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, windowWidth, windowHeight, 0, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    if (maxFrames == 0) {
        // No data loaded - show error message
        glColor3f(1.0f, 0.0f, 0.0f);
        // Could draw error text here
        return;
    }

    if (showBothSims && !threadedFrames.empty() && !mpiFrames.empty()) {
        // Draw both simulations side by side
        renderSimulation(threadedFrames, 0, windowWidth / 2.0f);
        renderSimulation(mpiFrames, windowWidth / 2.0f, windowWidth / 2.0f);

        // Draw separator line
        glColor3f(0.5f, 0.5f, 0.5f);
        glLineWidth(2.0f);
        glBegin(GL_LINES);
        glVertex2f(windowWidth / 2.0f, 0);
        glVertex2f(windowWidth / 2.0f, windowHeight);
        glEnd();

        // Draw labels
        glColor3f(1.0f, 1.0f, 1.0f);
        // Simple label indicators (rectangles)
        glBegin(GL_QUADS);
        // Threaded label indicator
        glVertex2f(10, 10);
        glVertex2f(100, 10);
        glVertex2f(100, 25);
        glVertex2f(10, 25);
        // MPI label indicator
        glVertex2f(windowWidth / 2.0f + 10, 10);
        glVertex2f(windowWidth / 2.0f + 100, 10);
        glVertex2f(windowWidth / 2.0f + 100, 25);
        glVertex2f(windowWidth / 2.0f + 10, 25);
        glEnd();

    }
    else if (!threadedFrames.empty()) {
        renderSimulation(threadedFrames, 0, windowWidth);
        // Label indicator
        glColor3f(1.0f, 1.0f, 1.0f);
        glBegin(GL_QUADS);
        glVertex2f(10, 10);
        glVertex2f(150, 10);
        glVertex2f(150, 25);
        glVertex2f(10, 25);
        glEnd();
    }
    else if (!mpiFrames.empty()) {
        renderSimulation(mpiFrames, 0, windowWidth);
        // Label indicator
        glColor3f(1.0f, 1.0f, 1.0f);
        glBegin(GL_QUADS);
        glVertex2f(10, 10);
        glVertex2f(100, 10);
        glVertex2f(100, 25);
        glVertex2f(10, 25);
        glEnd();
    }

    renderUI();
}

void Visualizer::renderSimulation(const std::vector<SimulationFrame>& frames, float offsetX, float width) {
    if (frames.empty() || currentFrame >= static_cast<int>(frames.size())) {
        return;
    }

    const auto& frame = frames[currentFrame];
    renderBodies(frame.bodies, offsetX, width);
}

void Visualizer::renderBodies(const std::vector<BodyState>& bodies, float offsetX, float width) {
    for (const auto& body : bodies) {
        Vec2 screenPos = worldToScreen(body.position, offsetX, width);

        // Skip bodies outside viewport
        if (screenPos.x < offsetX - 50 || screenPos.x > offsetX + width + 50 ||
            screenPos.y < -50 || screenPos.y > windowHeight + 50) {
            continue;
        }

        // Get color for this body
        Color color = Color(1.0f, 1.0f, 1.0f, 1.0f);  // Default white
        auto it = bodyColors.find(body.id);
        if (it != bodyColors.end()) {
            color = it->second;
        }

        // Scale radius for screen display
        float screenRadius = static_cast<float>(body.radius * viewScale);
        screenRadius = std::max(3.0f, std::min(50.0f, screenRadius)); // Clamp radius

        drawCircle(screenPos.x, screenPos.y, screenRadius, color);
    }
}

Vec2 Visualizer::worldToScreen(const Vec2& worldPos, float offsetX, float width) {
    if (maxX == minX || maxY == minY) {
        return Vec2(offsetX + width / 2, windowHeight / 2);
    }

    // Map world coordinates to screen coordinates
    double worldWidth = maxX - minX;
    double worldHeight = maxY - minY;

    // Normalize coordinates
    double normalizedX = (worldPos.x - minX) / worldWidth;
    double normalizedY = (worldPos.y - minY) / worldHeight;

    // Calculate scale to maintain aspect ratio
    double scaleX = width / worldWidth;
    double scaleY = windowHeight / worldHeight;
    double scale = std::min(scaleX, scaleY) * 0.9; // Use 90% of available space

    // Center the simulation in the viewport
    double scaledWidth = worldWidth * scale;
    double scaledHeight = worldHeight * scale;

    double centerX = offsetX + width / 2.0;
    double centerY = windowHeight / 2.0;

    return Vec2(
        centerX + (worldPos.x - (minX + maxX) / 2.0) * scale,
        centerY - (worldPos.y - (minY + maxY) / 2.0) * scale  // Flip Y for screen coordinates
    );
}

void Visualizer::drawCircle(float x, float y, float radius, const Color& color) {
    // Draw filled circle
    glColor4f(color.r, color.g, color.b, color.a);

    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x, y); // Center

    int segments = std::max(8, static_cast<int>(radius)); // More segments for smoother circles
    for (int i = 0; i <= segments; i++) {
        double angle = 2.0 * M_PI * i / segments;
        glVertex2f(x + radius * cos(angle), y + radius * sin(angle));
    }
    glEnd();

    // Draw black outline
    glColor3f(0.0f, 0.0f, 0.0f);
    glLineWidth(1.0f);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < segments; i++) {
        double angle = 2.0 * M_PI * i / segments;
        glVertex2f(x + radius * cos(angle), y + radius * sin(angle));
    }
    glEnd();
}

void Visualizer::renderUI() {
    // Draw UI background
    glColor4f(0.0f, 0.0f, 0.0f, 0.8f);
    glBegin(GL_QUADS);
    glVertex2f(10, windowHeight - 80);
    glVertex2f(400, windowHeight - 80);
    glVertex2f(400, windowHeight - 10);
    glVertex2f(10, windowHeight - 10);
    glEnd();

    // Draw UI border
    glColor3f(1.0f, 1.0f, 1.0f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(10, windowHeight - 80);
    glVertex2f(400, windowHeight - 80);
    glVertex2f(400, windowHeight - 10);
    glVertex2f(10, windowHeight - 10);
    glEnd();

    // Frame progress bar
    if (maxFrames > 1) {
        float progress = static_cast<float>(currentFrame) / (maxFrames - 1);

        // Progress bar background
        glColor3f(0.3f, 0.3f, 0.3f);
        glBegin(GL_QUADS);
        glVertex2f(20, windowHeight - 50);
        glVertex2f(380, windowHeight - 50);
        glVertex2f(380, windowHeight - 35);
        glVertex2f(20, windowHeight - 35);
        glEnd();

        // Progress bar fill
        glColor3f(0.0f, 1.0f, 0.0f);
        glBegin(GL_QUADS);
        glVertex2f(20, windowHeight - 50);
        glVertex2f(20 + 360 * progress, windowHeight - 50);
        glVertex2f(20 + 360 * progress, windowHeight - 35);
        glVertex2f(20, windowHeight - 35);
        glEnd();

        // Progress bar outline
        glColor3f(1.0f, 1.0f, 1.0f);
        glLineWidth(1.0f);
        glBegin(GL_LINE_LOOP);
        glVertex2f(20, windowHeight - 50);
        glVertex2f(380, windowHeight - 50);
        glVertex2f(380, windowHeight - 35);
        glVertex2f(20, windowHeight - 35);
        glEnd();
    }

    // Frame counter dots (simple visualization)
    glColor3f(1.0f, 1.0f, 1.0f);
    for (int i = 0; i < 5; i++) {
        glBegin(GL_QUADS);
        glVertex2f(20 + i * 8, windowHeight - 25);
        glVertex2f(25 + i * 8, windowHeight - 25);
        glVertex2f(25 + i * 8, windowHeight - 20);
        glVertex2f(20 + i * 8, windowHeight - 20);
        glEnd();
    }
}

void Visualizer::updateAnimation() {
    if (!isPlaying || maxFrames <= 1) return;

    double currentTime = glfwGetTime();
    double frameTime = 1.0 / (5.0 * animationSpeed); // 5 FPS base speed

    if (currentTime - lastFrameTime > frameTime) {
        currentFrame++;
        if (currentFrame >= maxFrames) {
            currentFrame = 0; // Loop animation
        }
        lastFrameTime = currentTime;

        // Print frame info to console every 10 frames
        if (currentFrame % 10 == 0) {
            std::cout << "\rFrame: " << currentFrame << "/" << (maxFrames - 1) << std::flush;
        }
    }
}

void Visualizer::handleInput() {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }
}

void Visualizer::keyCallback(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/) {
    Visualizer* vis = static_cast<Visualizer*>(glfwGetWindowUserPointer(window));

    if (action == GLFW_PRESS) {
        switch (key) {
        case GLFW_KEY_SPACE:
            vis->isPlaying = !vis->isPlaying;
            std::cout << "\n" << (vis->isPlaying ? "Playing" : "Paused") << std::endl;
            break;
        case GLFW_KEY_R:
            vis->currentFrame = 0;
            std::cout << "\nReset to frame 0" << std::endl;
            break;
        case GLFW_KEY_RIGHT:
            vis->currentFrame = std::min(vis->currentFrame + 1, vis->maxFrames - 1);
            std::cout << "\nFrame: " << vis->currentFrame << std::endl;
            break;
        case GLFW_KEY_LEFT:
            vis->currentFrame = std::max(vis->currentFrame - 1, 0);
            std::cout << "\nFrame: " << vis->currentFrame << std::endl;
            break;
        case GLFW_KEY_UP:
            vis->animationSpeed = std::min(vis->animationSpeed * 2, 10000.0);
            std::cout << "\nAnimation speed: " << vis->animationSpeed << "x" << std::endl;
            break;
        case GLFW_KEY_DOWN:
            vis->animationSpeed = std::max(vis->animationSpeed / 2, 0.1);
            std::cout << "\nAnimation speed: " << vis->animationSpeed << "x" << std::endl;
            break;
        case GLFW_KEY_1:
            vis->viewScale = 1.0;
            std::cout << "\nReset zoom" << std::endl;
            break;
        }
    }
}

void Visualizer::scrollCallback(GLFWwindow* window, double /*xoffset*/, double yoffset) {
    Visualizer* vis = static_cast<Visualizer*>(glfwGetWindowUserPointer(window));
    vis->viewScale *= (yoffset > 0) ? 1.1 : 0.9;
    vis->viewScale = std::max(0.1, std::min(vis->viewScale, 10.0));
}

void Visualizer::run() {
    std::cout << "\n=== OpenGL Visualization Controls ===" << std::endl;
    std::cout << "SPACE: Play/Pause animation" << std::endl;
    std::cout << "R: Reset to beginning" << std::endl;
    std::cout << "LEFT/RIGHT: Step through frames manually" << std::endl;
    std::cout << "UP/DOWN: Increase/Decrease animation speed" << std::endl;
    std::cout << "Mouse wheel: Zoom in/out" << std::endl;
    std::cout << "1: Reset zoom to 1x" << std::endl;
    std::cout << "ESC: Exit" << std::endl;
    std::cout << "====================================\n" << std::endl;

    while (!shouldClose()) {
        pollEvents();
        handleInput();
        updateAnimation();
        render();
        swapBuffers();

        // Check for OpenGL errors
        GLenum error = glGetError();
        if (error != GL_NO_ERROR) {
            std::cerr << "OpenGL Error: " << error << std::endl;
        }
    }
}

bool Visualizer::shouldClose() const {
    return glfwWindowShouldClose(window);
}

void Visualizer::swapBuffers() {
    glfwSwapBuffers(window);
}

void Visualizer::pollEvents() {
    glfwPollEvents();
}

