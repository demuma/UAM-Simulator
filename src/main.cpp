#ifdef __APPLE__
#include <OpenGL/gl3.h>
#else
#include <GL/glew.h>
#endif

#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>
#include <iostream>
#include <vector>
#include <cmath>

// Shader sources
const std::string vertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec3 aColor;

    out vec3 vColor;

    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;

    void main()
    {
        vColor = aColor;
        gl_Position = projection * view * model * vec4(aPos, 1.0);
    }
)";

const std::string fragmentShaderSource = R"(
    #version 330 core
    in vec3 vColor;
    out vec4 FragColor;

    void main()
    {
        FragColor = vec4(vColor, 1.0);
    }
)";

// Function to check for OpenGL errors
void checkOpenGLError(const std::string& label) {
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "OpenGL Error (" << label << "): ";
        switch (error) {
        case GL_INVALID_ENUM: std::cerr << "GL_INVALID_ENUM"; break;
        case GL_INVALID_VALUE: std::cerr << "GL_INVALID_VALUE"; break;
        case GL_INVALID_OPERATION: std::cerr << "GL_INVALID_OPERATION"; break;
        case GL_STACK_OVERFLOW: std::cerr << "GL_STACK_OVERFLOW"; break;
        case GL_STACK_UNDERFLOW: std::cerr << "GL_STACK_UNDERFLOW"; break;
        case GL_OUT_OF_MEMORY: std::cerr << "GL_OUT_OF_MEMORY"; break;
        case GL_INVALID_FRAMEBUFFER_OPERATION: std::cerr << "GL_INVALID_FRAMEBUFFER_OPERATION"; break;
        default: std::cerr << "Unknown error"; break;
        }
        std::cerr << std::endl;
    }
}

// Utility to generate a grid on the XZ-plane.
std::vector<float> generateGridVertices(int halfSize = 10) {
    // Lines parallel to X and Z.
    // Each line: two endpoints with positions and colors.
    // We'll color the grid lines gray and the center lines a bit darker.
    std::vector<float> vertices;
    float lineColor[3] = {0.7f, 0.7f, 0.7f};
    float centerColor[3] = {0.3f, 0.3f, 0.3f};

    // Lines along Z (varying x, from -halfSize to halfSize at z = constant)
    for (int x = -halfSize; x <= halfSize; x++) {
        bool isCenter = (x == 0);
        float r = isCenter ? centerColor[0] : lineColor[0];
        float g = isCenter ? centerColor[1] : lineColor[1];
        float b = isCenter ? centerColor[2] : lineColor[2];

        // Line from (x,0,-halfSize) to (x,0,halfSize)
        vertices.push_back(static_cast<float>(x)); vertices.push_back(0.0f); vertices.push_back(static_cast<float>(-halfSize));
        vertices.push_back(r); vertices.push_back(g); vertices.push_back(b);

        vertices.push_back(static_cast<float>(x)); vertices.push_back(0.0f); vertices.push_back(static_cast<float>(halfSize));
        vertices.push_back(r); vertices.push_back(g); vertices.push_back(b);
    }

    // Lines along X (varying z, from -halfSize to halfSize at x = constant)
    for (int z = -halfSize; z <= halfSize; z++) {
        bool isCenter = (z == 0);
        float r = isCenter ? centerColor[0] : lineColor[0];
        float g = isCenter ? centerColor[1] : lineColor[1];
        float b = isCenter ? centerColor[2] : lineColor[2];

        // Line from (-halfSize,0,z) to (halfSize,0,z)
        vertices.push_back(static_cast<float>(-halfSize)); vertices.push_back(0.0f); vertices.push_back(static_cast<float>(z));
        vertices.push_back(r); vertices.push_back(g); vertices.push_back(b);

        vertices.push_back(static_cast<float>(halfSize)); vertices.push_back(0.0f); vertices.push_back(static_cast<float>(z));
        vertices.push_back(r); vertices.push_back(g); vertices.push_back(b);
    }

    return vertices;
}

int main() {
    // --- SFML Window Setup ---
    sf::ContextSettings settings;
    settings.depthBits = 24;
    settings.stencilBits = 8;
    settings.antialiasingLevel = 16;
    settings.majorVersion = 3;
    settings.minorVersion = 3;
    settings.attributeFlags = sf::ContextSettings::Core; // Core profile

    sf::RenderWindow window(sf::VideoMode(800, 600), "3D Engine with SFML - Enhanced Controls", sf::Style::Default, settings);
    window.setVerticalSyncEnabled(true);
    window.setFramerateLimit(60);
    window.setActive(true);

#ifndef __APPLE__
    // Initialize GLEW on non-Apple platforms:
    glewExperimental = GL_TRUE;
    GLenum glewStatus = glewInit();
    if (glewStatus != GLEW_OK) {
        std::cerr << "Error initializing GLEW: " << glewGetErrorString(glewStatus) << std::endl;
        return -1;
    }
#endif

    // --- Shader Program ---
    // Vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    {
        const GLchar* vss = vertexShaderSource.c_str();
        glShaderSource(vertexShader, 1, &vss, NULL);
        glCompileShader(vertexShader);
    }
    checkOpenGLError("Compile Vertex Shader");

    GLint success;
    GLchar infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    // Fragment shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    {
        const GLchar* fss = fragmentShaderSource.c_str();
        glShaderSource(fragmentShader, 1, &fss, NULL);
        glCompileShader(fragmentShader);
    }
    checkOpenGLError("Compile Fragment Shader");

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    // Link program
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    checkOpenGLError("Link Shader Program");

    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLint modelLoc = glGetUniformLocation(shaderProgram, "model");
    GLint viewLoc = glGetUniformLocation(shaderProgram, "view");
    GLint projectionLoc = glGetUniformLocation(shaderProgram, "projection");

    // --- Cube Data ---
    float cubeVertices[] = {
        // Positions        // Colors
        -0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.0f, // Red
         0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 0.0f, // Green
         0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f, // Blue
        -0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 0.0f, // Yellow

        -0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 1.0f, // Cyan
         0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 1.0f, // Magenta
         0.5f,  0.5f,  0.5f,  1.0f, 0.5f, 0.5f, // Pink-ish
        -0.5f,  0.5f,  0.5f,  0.5f, 1.0f, 0.5f  // Lime-ish
    };

    unsigned int cubeIndices[] = {
        // Back face
        0, 1, 2,
        2, 3, 0,
        // Front face
        4, 5, 6,
        6, 7, 4,
        // Left face
        0, 3, 7,
        7, 4, 0,
        // Right face
        1, 5, 6,
        6, 2, 1,
        // Bottom face
        0, 1, 5,
        5, 4, 0,
        // Top face
        3, 2, 6,
        6, 7, 3
    };

    // Create VAO for cube
    GLuint cubeVAO;
    glGenVertexArrays(1, &cubeVAO);
    glBindVertexArray(cubeVAO);

    // Create VBO for cube
    GLuint cubeVBO;
    glGenBuffers(1, &cubeVBO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);

    // Create EBO for cube
    GLuint cubeEBO;
    glGenBuffers(1, &cubeEBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW);

    // Vertex attribute pointers for cube
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    checkOpenGLError("Cube position attrib");

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    checkOpenGLError("Cube color attrib");

    glBindVertexArray(0);

    // --- Grid Data ---
    std::vector<float> gridVertices = generateGridVertices(10);
    GLuint gridVAO, gridVBO;
    glGenVertexArrays(1, &gridVAO);
    glBindVertexArray(gridVAO);

    glGenBuffers(1, &gridVBO);
    glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
    glBufferData(GL_ARRAY_BUFFER, gridVertices.size() * sizeof(float), gridVertices.data(), GL_STATIC_DRAW);

    // Vertex attributes for grid
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    checkOpenGLError("Grid position attrib");

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    checkOpenGLError("Grid color attrib");

    glBindVertexArray(0);

    // --- Camera Setup ---
    glm::vec3 cameraPos = glm::vec3(0.0f, 1.0f, 3.0f);
    glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

    // Enable adjusted cameraUp (regarding body-fixed coordinate system)
    bool enableCameraUpAdjusted = false;

    // We'll use yaw/pitch to control the direction.
    float yaw = -90.0f; // Facing negative z by default
    float pitch = 0.0f;

    // Compute cameraFront from yaw/pitch
    auto updateCameraFront = [&](float yaw, float pitch) -> glm::vec3 {
        glm::vec3 front;
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        return glm::normalize(front);
    };
    glm::vec3 cameraFront = updateCameraFront(yaw, pitch);

    // Initialize cameraRight and cameraUpAdjusted (for body-fixed )
    glm::vec3 cameraRight = glm::normalize(glm::cross(cameraFront, cameraUp));
    glm::vec3 cameraUpAdjusted = glm::normalize(glm::cross(cameraRight, cameraFront));

    // Mouse look toggle
    bool enableMouseLook = false;
    window.setMouseCursorVisible(true);

    // Mouse center
    sf::Vector2i windowCenter(window.getSize().x / 2, window.getSize().y / 2);

    // --- Projection Matrix ---
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), 800.f / 600.f, 0.1f, 100.0f);

    // Enable depth test
    glEnable(GL_DEPTH_TEST);

    // Initialize delta clock
    sf::Clock deltaClock;

    // Define movement speed (units per second)
    float velocity = 5.0f;

    sf::Clock clock; // to rotate the cube over time

    while (window.isOpen()) {
        // --- Calculate delta time ---
        float deltaTime = deltaClock.restart().asSeconds();

        // --- Event Handling ---
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            }

            // Toggle mouse look
            if (event.type == sf::Event::KeyPressed) {
                if(event.key.code == sf::Keyboard::M) {
                    enableMouseLook = !enableMouseLook;
                    if (enableMouseLook) {
                        window.setMouseCursorVisible(false);
                        sf::Mouse::setPosition(windowCenter, window);
                    } else {
                        window.setMouseCursorVisible(true);
                    }
                } else if (event.key.code == sf::Keyboard::U) {
                    enableCameraUpAdjusted = !enableCameraUpAdjusted;
                    std::cout << "Camera Up Adjusted: " << (enableCameraUpAdjusted ? "Enabled" : "Disabled") << std::endl;
                }
            }

            // Handle quitting via Q key
            if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) {
                window.close();
            }
        }

        // --- Mouse Look Handling ---
        if (enableMouseLook) {
            sf::Vector2i mousePos = sf::Mouse::getPosition(window);
            float xoffset = static_cast<float>(mousePos.x - windowCenter.x);
            float yoffset = static_cast<float>(windowCenter.y - mousePos.y); // Reversed since y-coordinates go from top to bottom
            sf::Mouse::setPosition(windowCenter, window);

            float sensitivity = 0.1f;
            xoffset *= sensitivity;
            yoffset *= sensitivity;

            yaw += xoffset;
            pitch += yoffset;

            // Constrain pitch to prevent screen flip
            if (pitch > 89.0f) pitch = 89.0f;
            if (pitch < -89.0f) pitch = -89.0f;

            cameraFront = updateCameraFront(yaw, pitch);
            cameraRight = glm::normalize(glm::cross(cameraFront, cameraUp));
            cameraUpAdjusted = glm::normalize(glm::cross(cameraRight, cameraFront));
        }

        // --- Movement Handling ---
        glm::vec3 movement(0.0f);

        // Camera movement using keyboard inputs
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::W)) {
            movement += cameraFront;
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::S)) {
            movement -= cameraFront;
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::A)) {
            movement -= cameraRight;
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::D)) {
            movement += cameraRight;
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::C)) {
            movement += (enableCameraUpAdjusted ? cameraUpAdjusted : cameraUp);
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::V)) {
            movement -= (enableCameraUpAdjusted ? cameraUpAdjusted : cameraUp);
        }

        // Apply movement with normalization and deltaTime
        if (glm::length(movement) != 0.0f) {
            movement = glm::normalize(movement) * velocity * deltaTime;
            cameraPos += movement;
        }

        // --- Transformations ---
        glm::mat4 model = glm::mat4(1.0f);
        // Move and rotate the cube over time
        model = glm::translate(model, glm::vec3(0.0f, 1.0f, -5.0f));
        float angle = clock.getElapsedTime().asSeconds();
        model = glm::rotate(model, angle * glm::radians(20.0f), glm::vec3(1.0f, 1.0f, 0.0f));

        // Update view matrix
        glm::mat4 view;
        if (enableCameraUpAdjusted) {
            view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUpAdjusted);
        } else {
            view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        }

        // --- Drawing ---
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f); // White background
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        // Set uniform matrices
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));

        // Draw the cube
        glBindVertexArray(cubeVAO);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        checkOpenGLError("Draw Cube");

        // Draw the grid (model = identity for the grid)
        glm::mat4 gridModel = glm::mat4(1.0f);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(gridModel));
        glBindVertexArray(gridVAO);
        // Each line: 2 vertices. We have (21 lines in x-dir + 21 lines in z-dir) * 2 vertices each = 84 vertices total.
        // Draw using GL_LINES
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(gridVertices.size() / 6)); // each vertex has 6 floats (pos + color)
        checkOpenGLError("Draw Grid");

        glBindVertexArray(0);
        window.display();
    }

    // --- Cleanup ---
    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteBuffers(1, &cubeEBO);

    glDeleteVertexArrays(1, &gridVAO);
    glDeleteBuffers(1, &gridVBO);

    glDeleteProgram(shaderProgram);

    return 0;
}
