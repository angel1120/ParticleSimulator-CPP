#include <SFML/Graphics.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/System/Time.hpp>
#include "imgui.h"
#include "imgui-SFML.h"
#include <vector>
#include <cmath>
#include <random>
#include <iostream>
#include <string>
#include <chrono>
#include <mutex>

template<typename T>
const T& clamp(const T& value, const T& min, const T& max) {
    return (value < min) ? min : ((max < value) ? max : value);
}

// Particle class definition
class Particle {
public:
    Particle(float startX, float startY, float speed, float angle)
        : position(startX, startY), velocity(speed* std::cos(angle), speed* std::sin(angle)), isCollided(false) {}

    // Copy constructor
    Particle(const Particle& other)
        : position(other.position), velocity(other.velocity) {}

    // Assignment operator
    Particle& operator=(const Particle& other) {
        if (this != &other) {
            std::lock(mutex, other.mutex); // Lock both mutexes to avoid deadlock
            std::lock_guard<std::mutex> self_lock(mutex, std::adopt_lock);
            std::lock_guard<std::mutex> other_lock(other.mutex, std::adopt_lock);
            position = other.position;
            velocity = other.velocity;
        }
        return *this;
    }

    // Update the particle's position based on velocity
    void update(float deltaTime, int canvasWidth, int canvasHeight, const std::vector<sf::VertexArray>& walls) {
        std::lock_guard<std::mutex> lock(mutex);

        sf::Vector2f nextPosition = position + velocity * deltaTime;

        // Check collision with canvas edges
        if (nextPosition.x < 0 || nextPosition.x > canvasWidth) {
            velocity.x = -velocity.x;
            nextPosition.x = clamp(nextPosition.x, 0.0f, static_cast<float>(canvasWidth));
        }
        if (nextPosition.y < 0 || nextPosition.y > canvasHeight) {
            velocity.y = -velocity.y;
            nextPosition.y = clamp(nextPosition.y, 0.0f, static_cast<float>(canvasHeight));
        }
        if (!isCollided) {
            for (const auto& wall : walls) {
                for (size_t i = 0; i < wall.getVertexCount() - 1; ++i) {
                    sf::Vector2f p1 = wall[i].position;
                    sf::Vector2f p2 = wall[i + 1].position;
                    if (intersects(position, nextPosition, p1, p2)) {
                        isCollided = true;

                        //std::cout << "Collision detected!" << std::endl;
                        // Compute reflection vector
                        sf::Vector2f normal = getNormal(p1, p2);


                        // Ensure the normal is correctly normalized
                        // sf::Vector2f normalized = normal / std::sqrt(normal.x * normal.x + normal.y * normal.y);

                        float dotProduct = velocity.x * normal.x + velocity.y * normal.y;
                        //std::cout << "dotProduct: " << dotProduct << " normal.x: " << normal.x << " normal.y: " << normal.y << std::endl;
                        sf::Vector2f reflection = velocity - 2.0f * dotProduct * normal;
                        //std::cout << "reflection.x: " << reflection.x << " reflection.y: " << reflection.y << std::endl;
                        // Update next position
                        nextPosition = getCollisionPoint(position, nextPosition, p1, p2);
                        //std::cout << "position.x: " << position.x << " position.y: " << position.y << " nextPosition.x: " << nextPosition.x << " nextPosition.y: " << nextPosition.y << std::endl;

                        // Update velocity while maintaining magnitude
                        velocity = reflection;
                        break;
                    }
                }
            }
        }
        else {
            isCollided = false;
        }

        //std::cout << "position.x: " << position.x << " position.y: " << position.y << " velocity.x: " << velocity.x << " velocity.y: " << velocity.y << std::endl;

        // Update position
        position = nextPosition;
    }


    // Getter for position
    sf::Vector2f getPosition() const {
        return position;
    }

    // Getter for velocity
    sf::Vector2f getVelocity() const {
        return velocity;
    }

    // Render the particle on the given render window
    void render(sf::RenderWindow& window) const {
        std::lock_guard<std::mutex> lock(mutex);

        sf::CircleShape shape(5.0f);
        shape.setFillColor(sf::Color::Green);
        shape.setPosition(position);
        window.draw(shape);
    }

private:
    sf::Vector2f position;
    sf::Vector2f velocity;
    mutable std::mutex mutex; // Mutex for thread safety
    bool isCollided; // Flag to indicate whether a collision has been detected in the previous update

    // Function to get the collision point between a particle and a wall segment
    sf::Vector2f getCollisionPoint(const sf::Vector2f& startPos, const sf::Vector2f& endPos, const sf::Vector2f& wallStart, const sf::Vector2f& wallEnd) {
        sf::Vector2f collisionPoint;

        float x1 = startPos.x, y1 = startPos.y;
        float x2 = endPos.x, y2 = endPos.y;
        float x3 = wallStart.x, y3 = wallStart.y;
        float x4 = wallEnd.x, y4 = wallEnd.y;

        float denom = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);

        // Check if the lines are parallel
        if (denom == 0) {
            // If the lines are parallel, return the end position
            return endPos;
        }

        float t = ((x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4)) / denom;
        float u = -((x1 - x2) * (y1 - y3) - (y1 - y2) * (x1 - x3)) / denom;

        // Check if the intersection point lies within the line segments
        if (t >= 0 && t <= 1 && u >= 0 && u <= 1) {
            collisionPoint.x = x1 + t * (x2 - x1);
            collisionPoint.y = y1 + t * (y2 - y1);
        }
        else {
            // If the intersection point lies outside the line segments, return the end position
            return endPos;
        }

        return collisionPoint;
    }



    static bool intersects(const sf::Vector2f& p1, const sf::Vector2f& p2, const sf::Vector2f& q1, const sf::Vector2f& q2) {
        float s1_x, s1_y, s2_x, s2_y;
        s1_x = p2.x - p1.x;     s1_y = p2.y - p1.y;
        s2_x = q2.x - q1.x;     s2_y = q2.y - q1.y;
        float s, t;
        s = (-s1_y * (p1.x - q1.x) + s1_x * (p1.y - q1.y)) / (-s2_x * s1_y + s1_x * s2_y);
        t = (s2_x * (p1.y - q1.y) - s2_y * (p1.x - q1.x)) / (-s2_x * s1_y + s1_x * s2_y);
        return s >= 0 && s <= 1 && t >= 0 && t <= 1;
    }

    sf::Vector2f getNormal(const sf::Vector2f& p1, const sf::Vector2f& p2) {
        // Compute the direction vector of the wall segment
        sf::Vector2f direction = p2 - p1;

        // Compute the normal vector
        sf::Vector2f normal(-direction.y, direction.x); // Rotate direction vector 90 degrees

        // Normalize the normal vector
        float length = std::sqrt(normal.x * normal.x + normal.y * normal.y);
        if (length != 0) {
            normal.x /= length;
            normal.y /= length;
        }

        return normal;
    }
};

int main() {
    // Create SFML window
    sf::RenderWindow window(sf::VideoMode(1280, 720), "Particle Bouncing Application");
    window.setFramerateLimit(70); // Limit frame rate to 60 FPS

    sf::Font font; // Default font provided by SFML


    sf::Clock clock;
    sf::Clock deltaClock;
    sf::Clock fpsClock;
    int frameCount = 0;
    float fps = 0;
    auto lastFpsTime = std::chrono::steady_clock::now();


    // Setup ImGui
    ImGui::SFML::Init(window);

    // Particle system parameters
    std::vector<Particle> particles;
    float canvasWidth = 1280.0f;
    float canvasHeight = 720.0f;
    float speed = 100.0f;
    float startAngle = 0.0f;
    float endAngle = 180.0f;
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
    float angle = 45.0f * M_PI / 180.0f; // Convert angle to radians
    int numParticles = 1;
    std::vector<sf::VertexArray> walls;

    bool isDrawingLine = false;
    // Line parameters
    sf::Vector2f lineStart(100.0f, 360.0f); // Default line start point
    sf::Vector2f lineEnd(1180.0f, 360.0f);  // Default line end point

    // Define spawn point for Angle Setting and Speed Setting
    sf::Vector2f spawnPoint(640.0f, 360.0f); // Default spawn point


    // Main loop

    while (window.isOpen()) {
        // Process events
        sf::Event event;
        while (window.pollEvent(event)) {
            ImGui::SFML::ProcessEvent(event);

            if (event.type == sf::Event::Closed) {
                window.close();
            }//the following code considers the wall feature, feel free to comment out
            else if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
                if (!ImGui::GetIO().WantCaptureMouse) {
                    if (!isDrawingLine) {
                        isDrawingLine = true;
                        lineStart = window.mapPixelToCoords(sf::Vector2i(event.mouseButton.x, event.mouseButton.y));
                    }
                    else {
                        isDrawingLine = false;
                        lineEnd = window.mapPixelToCoords(sf::Vector2i(event.mouseButton.x, event.mouseButton.y));
                        walls.emplace_back(sf::LinesStrip, 2);
                        walls.back()[0].position = lineStart;
                        walls.back()[1].position = lineEnd;
                    }
                }
            }
        }

        // Get elapsed time since last frame
        float deltaTime = clock.restart().asSeconds();

        // ImGui interface
        //ImGui::SFML::Update(window, sf::seconds(deltaTime));
        ImGui::SFML::Update(window, deltaClock.restart());

        ImGui::Begin("Particle Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        // Display FPS at the bottom of the setting UI

        ImGui::Separator();
        // FPS display
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastFpsTime).count() / 1000.0;
        if (elapsedTime >= 0.5) {
            float valid_fps = frameCount / elapsedTime;
            fps = valid_fps;
            frameCount = 0;
            lastFpsTime = currentTime;
        }
        ImGui::Text("FPS: %.1f", fps);

        ImGui::Separator();

        // Tabs for different settings
        if (ImGui::BeginTabBar("Settings Tabs")) {
            // Line Setting UI
            if (ImGui::BeginTabItem("Line Setting")) {
                ImGui::SliderFloat("Line Start X", &lineStart.x, 0.0f, canvasWidth);
                ImGui::SliderFloat("Line Start Y", &lineStart.y, 0.0f, canvasHeight);
                ImGui::SliderFloat("Line End X", &lineEnd.x, 0.0f, canvasWidth);
                ImGui::SliderFloat("Line End Y", &lineEnd.y, 0.0f, canvasHeight);
                ImGui::SliderFloat("Velocity", &speed, 50.0f, 500.0f);
                ImGui::SliderFloat("Angle (degrees)", &angle, 0.0f, 360.0f);
                ImGui::SliderInt("Number of Particles", &numParticles, 1, 10000);
                if (ImGui::Button("Generate Particles")) {
                    // Generate particles equally spaced along the line
                    particles.clear();
                    for (int i = 0; i < numParticles; ++i) {
                        float t = 0.0f;
                        if (numParticles > 1) {
                            t = static_cast<float>(i) / (numParticles - 1); // t ranges from 0 to 1
                        }
                        sf::Vector2f position = lineStart + t * (lineEnd - lineStart);
                        particles.emplace_back(position.x, position.y, speed, angle);
                    }
                }
                ImGui::EndTabItem();
            }

            // Angle Setting UI
            if (ImGui::BeginTabItem("Angle Setting")) {
                ImGui::SliderFloat("Spawn Point X", &lineStart.x, 0.0f, canvasWidth);
                ImGui::SliderFloat("Spawn Point Y", &lineStart.y, 0.0f, canvasHeight);
                ImGui::SliderAngle("Start Angle", &startAngle);
                ImGui::SliderAngle("End Angle", &endAngle);
                ImGui::SliderFloat("Velocity", &speed, 50.0f, 500.0f);
                ImGui::SliderInt("Number of Particles", &numParticles, 1, 10000);
                if (ImGui::Button("Generate Particles")) {
                    // Generate particles equally spaced along the angle
                    particles.clear();
                    float angleIncrement = 0.0f;
                    if (numParticles > 1) {
                        angleIncrement = (endAngle - startAngle) / (numParticles - 1);
                    }
                    for (int i = 0; i < numParticles; ++i) {
                        float currentAngle = startAngle + i * angleIncrement;
                        //float radians = currentAngle * 3.14159f / 180.0f; // Convert degrees to radians
                        sf::Vector2f position = lineStart;
                        particles.emplace_back(position.x, position.y, speed, currentAngle);
                    }
                }
                ImGui::EndTabItem();
            }

            // Speed Setting UI
            if (ImGui::BeginTabItem("Speed Setting")) {
                ImGui::SliderFloat("Spawn Point X", &lineStart.x, 0.0f, canvasWidth);
                ImGui::SliderFloat("Spawn Point Y", &lineStart.y, 0.0f, canvasHeight);
                ImGui::SliderFloat("Angle (degrees)", &angle, 0.0f, 360.0f);
                ImGui::SliderInt("Number of Particles", &numParticles, 1, 10000);
                if (ImGui::Button("Generate Particles")) {
                    // Generate particles with equally different speeds
                    particles.clear();
                    float speedIncrement = 450.0f / numParticles; // Speed ranges from 50 to 500
                    for (int i = 0; i < numParticles; ++i) {
                        float currentSpeed = 50.0f + i * speedIncrement;
                        sf::Vector2f position = lineStart;
                        particles.emplace_back(position.x, position.y, currentSpeed, angle);
                    }
                }
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
        if (ImGui::Button("Clear Particles")) {
            particles.clear();
        }
        if (ImGui::Button("Clear Walls")) {
            walls.clear();
        }
        if (ImGui::Button("Clear last wall")) {
            walls.pop_back();
        }


        ImGui::End();


        // Clear window
        window.clear(sf::Color::Black);
        for (auto& wall : walls) {
            window.draw(wall);
        }

        // Update particle system
        for (auto& particle : particles) {
            particle.update(deltaTime, canvasWidth, canvasHeight, walls);
        }

        // Render particles
        for (auto& particle : particles) {
            particle.render(window);
        }

        // Render ImGui interface
        ImGui::SFML::Render(window);

        // Display window
        window.display();

        frameCount++;
    }

    // Cleanup
    ImGui::SFML::Shutdown();

    return 0;
}
