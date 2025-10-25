// Fixed lighting and shadow version
#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include <optional>
#include <variant>
#include <yaml-cpp/yaml.h>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Generate grid vertices
std::vector<float> generateGridVertices(int halfSize = 10) {
    std::vector<float> vertices;
    float lineColor[3] = {0.7f, 0.7f, 0.7f};
    float centerColor[3] = {0.3f, 0.3f, 0.3f};

    for (int x = -halfSize; x <= halfSize; x++) {
        bool isCenter = (x == 0);
        float r = isCenter ? centerColor[0] : lineColor[0];
        float g = isCenter ? centerColor[1] : lineColor[1];
        float b = isCenter ? centerColor[2] : lineColor[2];

        vertices.insert(vertices.end(), {
            static_cast<float>(x), 0.0f, static_cast<float>(-halfSize), r, g, b,
            static_cast<float>(x), 0.0f, static_cast<float>(halfSize), r, g, b
        });
    }

    for (int z = -halfSize; z <= halfSize; z++) {
        bool isCenter = (z == 0);
        float r = isCenter ? centerColor[0] : lineColor[0];
        float g = isCenter ? centerColor[1] : lineColor[1];
        float b = isCenter ? centerColor[2] : lineColor[2];

        vertices.insert(vertices.end(), {
            static_cast<float>(-halfSize), 0.0f, static_cast<float>(z), r, g, b,
            static_cast<float>(halfSize), 0.0f, static_cast<float>(z), r, g, b
        });
    }

    return vertices;
}

// Draw a visual indicator for the light source
void drawLightSource(const glm::vec3& lightPos) {
    glDisable(GL_LIGHTING);
    glPushMatrix();
    glTranslatef(lightPos.x, lightPos.y, lightPos.z);
    
    // Draw a bright yellow sphere
    glColor3f(1.0f, 1.0f, 0.0f);
    
    // Simple icosphere approximation
    const int segments = 8;
    const float radius = 0.3f;
    
    for (int i = 0; i < segments; i++) {
        float theta1 = (float)i * 2.0f * M_PI / segments;
        float theta2 = (float)(i + 1) * 2.0f * M_PI / segments;
        
        glBegin(GL_TRIANGLE_STRIP);
        for (int j = 0; j <= segments/2; j++) {
            float phi = (float)j * M_PI / (segments/2);
            
            float x1 = radius * sin(phi) * cos(theta1);
            float y1 = radius * cos(phi);
            float z1 = radius * sin(phi) * sin(theta1);
            
            float x2 = radius * sin(phi) * cos(theta2);
            float y2 = radius * cos(phi);
            float z2 = radius * sin(phi) * sin(theta2);
            
            glVertex3f(x1, y1, z1);
            glVertex3f(x2, y2, z2);
        }
        glEnd();
    }
    
    glPopMatrix();
}

// Simple planar shadow projection
void drawSimpleShadow(const glm::vec3& objectPos, const glm::vec3& objectScale, const glm::vec3& lightPos) {
    // Only draw shadow if light is above the object
    if (lightPos.y <= objectPos.y) return;
    
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);  // Don't write to depth buffer
    
    glColor4f(0.0f, 0.0f, 0.0f, 0.3f);  // Semi-transparent black
    
    glPushMatrix();
    
    // Calculate shadow position on ground (Y = 0.01 to avoid z-fighting)
    float shadowY = 0.01f;
    
    // Calculate shadow offset based on light direction
    glm::vec3 lightDir = objectPos - lightPos;
    float t = (shadowY - lightPos.y) / lightDir.y;  // Parameter for ray-plane intersection
    
    float shadowX = lightPos.x + t * lightDir.x;
    float shadowZ = lightPos.z + t * lightDir.z;
    
    // Transform to shadow position
    glTranslatef(shadowX, shadowY, shadowZ);
    
    // Scale shadow based on object size and light height
    float lightHeight = lightPos.y - shadowY;
    float objectHeight = objectPos.y;
    float shadowScale = 1.0f + (objectHeight / lightHeight) * 0.5f; // Shadow gets bigger with height
    
    glScalef(objectScale.x * shadowScale, 0.01f, objectScale.z * shadowScale);
    
    // Draw flattened shadow
    glBegin(GL_QUADS);
    glVertex3f(-0.5f, 0.0f, -0.5f);
    glVertex3f( 0.5f, 0.0f, -0.5f);
    glVertex3f( 0.5f, 0.0f,  0.5f);
    glVertex3f(-0.5f, 0.0f,  0.5f);
    glEnd();
    
    glPopMatrix();
    
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}



// Simple Object3D with fixed lighting
class Object3D {
public:
    std::string name;
    glm::vec3 position;
    glm::vec3 dimensions;
    glm::vec3 color;

    Object3D(const YAML::Node& config) {
        name = config["name"].as<std::string>();
        position = glm::vec3(config["position"][0].as<float>(),
                             config["position"][1].as<float>(),
                             config["position"][2].as<float>());
        dimensions = glm::vec3(config["dimensions"][0].as<float>(),
                               config["dimensions"][1].as<float>(),
                               config["dimensions"][2].as<float>());
        color = glm::vec3(config["color"][0].as<float>(),
                          config["color"][1].as<float>(),
                          config["color"][2].as<float>());
    }

    void draw(const glm::vec3& lightPos) {
        glPushMatrix();
        glTranslatef(position.x, position.y, position.z);
        glScalef(dimensions.x, dimensions.y, dimensions.z);
        
        // Draw cube faces with manual lighting calculation
        drawLitCube(lightPos);
        
        // Draw edges
        drawCubeEdges();
        
        glPopMatrix();
    }
    
    void drawShadow(const glm::vec3& lightPos) {
        drawSimpleShadow(position, dimensions, lightPos);
    }

private:
    void drawLitCube(const glm::vec3& lightPos) {
        glDisable(GL_LIGHTING);  // We'll do lighting manually
        
        // Get world position of cube center
        glm::vec4 worldPos = glm::vec4(position, 1.0f);
        
        // Define face normals and vertices
        struct Face {
            glm::vec3 normal;
            glm::vec3 vertices[4];
        };
        
        Face faces[6] = {
            // Front face (+Z)
            { glm::vec3(0, 0, 1), {
                glm::vec3(-0.5f, -0.5f,  0.5f), glm::vec3( 0.5f, -0.5f,  0.5f),
                glm::vec3( 0.5f,  0.5f,  0.5f), glm::vec3(-0.5f,  0.5f,  0.5f)
            }},
            // Back face (-Z)
            { glm::vec3(0, 0, -1), {
                glm::vec3( 0.5f, -0.5f, -0.5f), glm::vec3(-0.5f, -0.5f, -0.5f),
                glm::vec3(-0.5f,  0.5f, -0.5f), glm::vec3( 0.5f,  0.5f, -0.5f)
            }},
            // Top face (+Y)
            { glm::vec3(0, 1, 0), {
                glm::vec3(-0.5f,  0.5f, -0.5f), glm::vec3(-0.5f,  0.5f,  0.5f),
                glm::vec3( 0.5f,  0.5f,  0.5f), glm::vec3( 0.5f,  0.5f, -0.5f)
            }},
            // Bottom face (-Y)
            { glm::vec3(0, -1, 0), {
                glm::vec3(-0.5f, -0.5f, -0.5f), glm::vec3( 0.5f, -0.5f, -0.5f),
                glm::vec3( 0.5f, -0.5f,  0.5f), glm::vec3(-0.5f, -0.5f,  0.5f)
            }},
            // Right face (+X)
            { glm::vec3(1, 0, 0), {
                glm::vec3( 0.5f, -0.5f, -0.5f), glm::vec3( 0.5f,  0.5f, -0.5f),
                glm::vec3( 0.5f,  0.5f,  0.5f), glm::vec3( 0.5f, -0.5f,  0.5f)
            }},
            // Left face (-X)
            { glm::vec3(-1, 0, 0), {
                glm::vec3(-0.5f, -0.5f,  0.5f), glm::vec3(-0.5f,  0.5f,  0.5f),
                glm::vec3(-0.5f,  0.5f, -0.5f), glm::vec3(-0.5f, -0.5f, -0.5f)
            }}
        };
        
        glBegin(GL_QUADS);
        
        for (int i = 0; i < 6; i++) {
            // Calculate lighting for this face
            glm::vec3 faceWorldPos = position; // Center of face (approximation)
            glm::vec3 lightDir = glm::normalize(lightPos - faceWorldPos);
            
            // Transform normal to world space (ignoring scale for simplicity)
            glm::vec3 worldNormal = faces[i].normal;
            
            // Calculate diffuse lighting
            float diffuse = std::max(0.0f, glm::dot(worldNormal, lightDir));
            
            // Apply lighting to base color
            float ambient = 0.3f;  // Ambient lighting
            float lightIntensity = ambient + diffuse * 0.7f;
            lightIntensity = std::min(lightIntensity, 1.0f);
            
            glColor3f(color.x * lightIntensity, color.y * lightIntensity, color.z * lightIntensity);
            
            // Draw the face
            for (int j = 0; j < 4; j++) {
                glVertex3f(faces[i].vertices[j].x, faces[i].vertices[j].y, faces[i].vertices[j].z);
            }
        }
        
        glEnd();
    }
    
    void drawCubeEdges() {
        glDisable(GL_LIGHTING);
        glColor4f(0.0f, 0.0f, 0.0f, 0.8f);
        glLineWidth(1.5f);
        
        glBegin(GL_LINES);
        
        // Front face edges
        glVertex3f(-0.5f, -0.5f,  0.5f); glVertex3f( 0.5f, -0.5f,  0.5f);
        glVertex3f( 0.5f, -0.5f,  0.5f); glVertex3f( 0.5f,  0.5f,  0.5f);
        glVertex3f( 0.5f,  0.5f,  0.5f); glVertex3f(-0.5f,  0.5f,  0.5f);
        glVertex3f(-0.5f,  0.5f,  0.5f); glVertex3f(-0.5f, -0.5f,  0.5f);
        
        // Back face edges
        glVertex3f(-0.5f, -0.5f, -0.5f); glVertex3f( 0.5f, -0.5f, -0.5f);
        glVertex3f( 0.5f, -0.5f, -0.5f); glVertex3f( 0.5f,  0.5f, -0.5f);
        glVertex3f( 0.5f,  0.5f, -0.5f); glVertex3f(-0.5f,  0.5f, -0.5f);
        glVertex3f(-0.5f,  0.5f, -0.5f); glVertex3f(-0.5f, -0.5f, -0.5f);
        
        // Connecting edges
        glVertex3f(-0.5f, -0.5f, -0.5f); glVertex3f(-0.5f, -0.5f,  0.5f);
        glVertex3f( 0.5f, -0.5f, -0.5f); glVertex3f( 0.5f, -0.5f,  0.5f);
        glVertex3f( 0.5f,  0.5f, -0.5f); glVertex3f( 0.5f,  0.5f,  0.5f);
        glVertex3f(-0.5f,  0.5f, -0.5f); glVertex3f(-0.5f,  0.5f,  0.5f);
        
        glEnd();
        glLineWidth(1.0f);
    }
};

int main() {
    std::cout << "=== UAM Simulator - Fixed Lighting ===" << std::endl;
    
    // Window settings
    sf::ContextSettings settings;
    settings.depthBits = 24;
    settings.stencilBits = 8;
    settings.antiAliasingLevel = 4;
    settings.majorVersion = 2;
    settings.minorVersion = 1;
    
    sf::RenderWindow window(sf::VideoMode({1200, 900}),
                           "UAM Simulator - Fixed Shadows", 
                           sf::Style::Default,
                           sf::State::Windowed,
                           settings);
    
    bool activeResult = window.setActive(true);
    if (!activeResult) {
        std::cerr << "Warning: Could not activate OpenGL context!" << std::endl;
    }
    
    window.setVerticalSyncEnabled(true);
    
    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "Renderer: " << glGetString(GL_RENDERER) << std::endl;
    
    // OpenGL settings
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Load objects
    std::vector<std::unique_ptr<Object3D>> objects;
    
    try {
        YAML::Node config = YAML::LoadFile("config.yaml");
        for (const auto& objectConfig : config["objects"]) {
            objects.push_back(std::make_unique<Object3D>(objectConfig));
        }
        std::cout << "Loaded " << objects.size() << " objects" << std::endl;
    } catch (const YAML::Exception& e) {
        std::cerr << "Error loading config.yaml, creating default objects..." << std::endl;
        
        YAML::Node defaultObj1;
        defaultObj1["name"] = "RedCube";
        defaultObj1["position"] = std::vector<float>{0, 1, 0};
        defaultObj1["dimensions"] = std::vector<float>{1, 1, 1};
        defaultObj1["color"] = std::vector<float>{1.0f, 0.0f, 0.0f};
        objects.push_back(std::make_unique<Object3D>(defaultObj1));
        
        YAML::Node defaultObj2;
        defaultObj2["name"] = "GreenCube";
        defaultObj2["position"] = std::vector<float>{3, 1, 2};
        defaultObj2["dimensions"] = std::vector<float>{1, 2, 1};
        defaultObj2["color"] = std::vector<float>{0.0f, 1.0f, 0.0f};
        objects.push_back(std::make_unique<Object3D>(defaultObj2));
        
        YAML::Node defaultObj3;
        defaultObj3["name"] = "BlueCube";
        defaultObj3["position"] = std::vector<float>{-2, 0.5f, 1};
        defaultObj3["dimensions"] = std::vector<float>{1, 0.5f, 1};
        defaultObj3["color"] = std::vector<float>{0.0f, 0.0f, 1.0f};
        objects.push_back(std::make_unique<Object3D>(defaultObj3));
    }

    // Generate grid
    std::vector<float> gridVertices = generateGridVertices(10);

    // Light and shadow settings
    glm::vec3 lightPosition(5.0f, 8.0f, 3.0f);
    bool showLightSource = true;
    bool enableShadows = true;

    // Camera setup
    glm::vec3 cameraPos = glm::vec3(0.0f, 3.0f, 8.0f);
    glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
    float yaw = -90.0f;
    float pitch = -10.0f; // Look down slightly
    const float yawSpeed = 60.0f;
    bool enableMouseLook = false;
    sf::Vector2i windowCenter(window.getSize().x / 2, window.getSize().y / 2);

    auto updateCameraFront = [&](float yaw, float pitch) -> glm::vec3 {
        glm::vec3 front;
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        return glm::normalize(front);
    };
    
    glm::vec3 cameraFront = updateCameraFront(yaw, pitch);
    glm::vec3 cameraRight = glm::normalize(glm::cross(cameraFront, cameraUp));
    float velocity = 5.0f;

    sf::Clock deltaClock;
    sf::Clock fpsCounter;
    int frameCount = 0;

    // Event handlers
    const auto onClose = [&](const sf::Event::Closed&) {
        window.close();
    };
    
    const auto onKeyPressed = [&](const sf::Event::KeyPressed& keyPressed) {
        if (keyPressed.scancode == sf::Keyboard::Scancode::M) {
            enableMouseLook = !enableMouseLook;
            window.setMouseCursorVisible(!enableMouseLook);
            if (enableMouseLook) {
                sf::Mouse::setPosition(windowCenter, window);
            }
            std::cout << "Mouse look: " << (enableMouseLook ? "ON" : "OFF") << std::endl;
        }
        else if (keyPressed.scancode == sf::Keyboard::Scancode::B) {
            showLightSource = !showLightSource;
            std::cout << "Light source visible: " << (showLightSource ? "ON" : "OFF") << std::endl;
        }
        else if (keyPressed.scancode == sf::Keyboard::Scancode::H) {
            enableShadows = !enableShadows;
            std::cout << "Shadows: " << (enableShadows ? "ON" : "OFF") << std::endl;
        }
        else if (keyPressed.scancode == sf::Keyboard::Scancode::Escape) {
            window.close();
        }
        
        // Light movement
        float lightSpeed = 0.5f;
        if (keyPressed.scancode == sf::Keyboard::Scancode::Up) lightPosition.z -= lightSpeed;
        if (keyPressed.scancode == sf::Keyboard::Scancode::Down) lightPosition.z += lightSpeed;
        if (keyPressed.scancode == sf::Keyboard::Scancode::Left) lightPosition.x -= lightSpeed;
        if (keyPressed.scancode == sf::Keyboard::Scancode::Right) lightPosition.x += lightSpeed;
        if (keyPressed.scancode == sf::Keyboard::Scancode::PageUp) lightPosition.y += lightSpeed;
        if (keyPressed.scancode == sf::Keyboard::Scancode::PageDown) lightPosition.y -= lightSpeed;
    };
    
    const auto onMouseMoved = [&](const sf::Event::MouseMoved& mouseMoved) {
        if (enableMouseLook) {
            float xoffset = static_cast<float>(mouseMoved.position.x - windowCenter.x);
            float yoffset = static_cast<float>(windowCenter.y - mouseMoved.position.y);
            sf::Mouse::setPosition(windowCenter, window);

            float sensitivity = 0.1f;
            yaw += xoffset * sensitivity;
            pitch += yoffset * sensitivity;

            if (pitch > 89.0f) pitch = 89.0f;
            if (pitch < -89.0f) pitch = -89.0f;

            cameraFront = updateCameraFront(yaw, pitch);
            cameraRight = glm::normalize(glm::cross(cameraFront, cameraUp));
        }
    };

    std::cout << "\n=== Controls ===" << std::endl;
    std::cout << "WASD: Move camera" << std::endl;
    std::cout << "C/V: Move up/down" << std::endl;
    std::cout << "Q/E: Rotate left/right" << std::endl;
    std::cout << "M: Toggle mouse look" << std::endl;
    std::cout << "Arrow Keys: Move light source" << std::endl;
    std::cout << "PageUp/PageDown: Move light up/down" << std::endl;
    std::cout << "B: Toggle light source visibility" << std::endl;
    std::cout << "H: Toggle shadows" << std::endl;
    std::cout << "ESC: Exit" << std::endl;

    // Main loop
    while (window.isOpen()) {
        float deltaTime = deltaClock.restart().asSeconds();
        if (deltaTime > 0.1f) deltaTime = 0.1f;

        // Handle events
        window.handleEvents(onClose,
                           [](const sf::Event::Resized&){},
                           onKeyPressed,
                           [](const sf::Event::KeyReleased&){},
                           [](const sf::Event::MouseButtonPressed&){},
                           [](const sf::Event::MouseButtonReleased&){},
                           onMouseMoved,
                           [](const sf::Event::MouseWheelScrolled&){});

        // Movement
        glm::vec3 movement(0.0f);
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) movement += cameraFront;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) movement -= cameraFront;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) movement -= cameraRight;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) movement += cameraRight;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::C)) movement += cameraUp;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::V)) movement -= cameraUp;

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::E)) {
            yaw += yawSpeed * deltaTime;
            cameraFront = updateCameraFront(yaw, pitch);
            cameraRight = glm::normalize(glm::cross(cameraFront, cameraUp));
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Q)) {
            yaw -= yawSpeed * deltaTime;
            cameraFront = updateCameraFront(yaw, pitch);
            cameraRight = glm::normalize(glm::cross(cameraFront, cameraUp));
        }

        if (glm::length(movement) > 0.0f) {
            movement = glm::normalize(movement) * velocity * deltaTime;
            cameraPos += movement;
        }

        // Rendering
        glClearColor(0.8f, 0.9f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Set projection matrix
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        float aspectRatio = static_cast<float>(window.getSize().x) / static_cast<float>(window.getSize().y);
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.5f, 50.0f);
        glLoadMatrixf(glm::value_ptr(projection));

        // Set view matrix
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        glLoadMatrixf(glm::value_ptr(view));

        // Draw grid
        glDisable(GL_LIGHTING);
        glBegin(GL_LINES);
        for (size_t i = 0; i < gridVertices.size(); i += 6) {
            glColor3f(gridVertices[i+3], gridVertices[i+4], gridVertices[i+5]);
            glVertex3f(gridVertices[i], gridVertices[i+1], gridVertices[i+2]);
        }
        glEnd();

        // Draw shadows first (on the ground)
        if (enableShadows) {
            for (const auto& object : objects) {
                object->drawShadow(lightPosition);
            }
        }

        // Draw objects with lighting
        for (const auto& object : objects) {
            object->draw(lightPosition);
        }

        // Draw light source indicator
        if (showLightSource) {
            drawLightSource(lightPosition);
        }

        window.display();
        
        // FPS counter
        frameCount++;
        if (fpsCounter.getElapsedTime().asSeconds() >= 1.0f) {
            std::cout << "FPS: " << frameCount << " | Light: (" 
                      << lightPosition.x << ", " << lightPosition.y << ", " << lightPosition.z << ")" << std::endl;
            frameCount = 0;
            fpsCounter.restart();
        }
    }

    std::cout << "Exiting successfully!" << std::endl;
    return 0;
}