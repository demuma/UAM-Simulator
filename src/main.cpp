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
#include <map>
#include <string>
#include <yaml-cpp/yaml.h>

// Shader sources

// Opaque Vertex Shader (Used for Faces)
const std::string opaqueVertexShaderSource = R"(
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

// Opaque Fragment Shader (Used for Faces)
const std::string opaqueFragmentShaderSource = R"(
    #version 330 core
    in vec3 vColor;
    out vec4 FragColor;

    void main()
    {
        FragColor = vec4(vColor, 1.0); // Fully opaque
    }
)";

// Transparent Fragment Shader (Used for Edges)
const std::string transparentFragmentShaderSource = R"(
    #version 330 core
    in vec3 vColor;
    out vec4 FragColor;

    void main()
    {
        FragColor = vec4(vColor, 0.7); // Semi-transparent
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

class Object3D {
public:
    std::string name;
    glm::vec3 position;
    glm::vec3 dimensions;
    glm::vec3 color; // Single color for the entire cube

    // Constructor to initialize from YAML data
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

        setupMesh(); // Initialize VAO, VBO, EBO
    }

    // Draw faces using opaque shader and edges using transparent shader
    void draw(GLuint opaqueShaderProgram, GLuint transparentShaderProgram, const glm::mat4& view, const glm::mat4& projection) {
        // Calculate model matrix (translation and scaling)
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, position);
        model = glm::scale(model, dimensions);

        // --- Draw Faces ---
        glUseProgram(opaqueShaderProgram);

        // Set uniforms
        GLint modelLoc = glGetUniformLocation(opaqueShaderProgram, "model");
        GLint viewLoc = glGetUniformLocation(opaqueShaderProgram, "view");
        GLint projectionLoc = glGetUniformLocation(opaqueShaderProgram, "projection");

        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));

        // Disable blending for opaque rendering
        glDisable(GL_BLEND);

        // Draw triangles (faces)
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        // --- Draw Edges ---
        glUseProgram(transparentShaderProgram);

        // Set uniforms
        GLint t_modelLoc = glGetUniformLocation(transparentShaderProgram, "model");
        GLint t_viewLoc = glGetUniformLocation(transparentShaderProgram, "view");
        GLint t_projectionLoc = glGetUniformLocation(transparentShaderProgram, "projection");

        glUniformMatrix4fv(t_modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(t_viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(t_projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));

        // Enable blending for transparent rendering
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // **Important**: Ensure edges are drawn on top of faces
        // Disable depth writing to allow lines to appear over faces
        glDepthMask(GL_FALSE);

        // Draw lines (edges)
        glBindVertexArray(VAO);
        glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, (void*)(36 * sizeof(GLuint)));
        glBindVertexArray(0);

        // Re-enable depth writing
        glDepthMask(GL_TRUE);

        // Disable blending after rendering edges
        glDisable(GL_BLEND);
    }

    // Destructor to clean up resources
    ~Object3D() {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
    }

private:
    GLuint VAO, VBO, EBO;

    void setupMesh() {
        // Vertex data for a unit cube (scaled later)
        float vertices[] = {
            // Positions             // Colors
            -0.5f, -0.5f, -0.5f,     color.x, color.y, color.z, // 0
             0.5f, -0.5f, -0.5f,     color.x, color.y, color.z, // 1
             0.5f,  0.5f, -0.5f,     color.x, color.y, color.z, // 2
            -0.5f,  0.5f, -0.5f,     color.x, color.y, color.z, // 3
            -0.5f, -0.5f,  0.5f,     color.x, color.y, color.z, // 4
             0.5f, -0.5f,  0.5f,     color.x, color.y, color.z, // 5
             0.5f,  0.5f,  0.5f,     color.x, color.y, color.z, // 6
            -0.5f,  0.5f,  0.5f,     color.x, color.y, color.z, // 7

            // Edge vertices
            -0.5f, -0.5f, -0.5f,     0.0f, 0.0f, 0.0f, // 8
             0.5f, -0.5f, -0.5f,     0.0f, 0.0f, 0.0f, // 9
             0.5f,  0.5f, -0.5f,     0.0f, 0.0f, 0.0f, // 10
            -0.5f,  0.5f, -0.5f,     0.0f, 0.0f, 0.0f, // 11
            -0.5f, -0.5f,  0.5f,     0.0f, 0.0f, 0.0f, // 12
             0.5f, -0.5f,  0.5f,     0.0f, 0.0f, 0.0f, // 13
             0.5f,  0.5f,  0.5f,     0.0f, 0.0f, 0.0f, // 14
            -0.5f,  0.5f,  0.5f,     0.0f, 0.0f, 0.0f  // 15
        };

        unsigned int indices[] = {
            // Face indices (12 triangles for 6 faces)
            0, 1, 2,
            2, 3, 0,
            4, 5, 6,
            6, 7, 4,
            0, 3, 7,
            7, 4, 0,
            1, 5, 6,
            6, 2, 1,
            0, 1, 5,
            5, 4, 0,
            3, 2, 6,
            6, 7, 3,

            // Edge indices (12 lines for edges)
            8, 9,
            9, 10,
            10, 11,
            11, 8,
            12, 13,
            13, 14,
            14, 15,
            15, 12,
            8, 12,
            9, 13,
            10, 14,
            11, 15
        };

        // Create VAO, VBO, EBO
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);

        // Vertex Buffer Object
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        // Element Buffer Object
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

        // Position attribute
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        // Color attribute
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glBindVertexArray(0);
    }
};

int main() {
    // --- SFML Window Setup ---
    sf::ContextSettings settings;
    settings.depthBits = 24;
    settings.stencilBits = 8;
    settings.antialiasingLevel = 16; // High-quality anti-aliasing
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

    // --- Shader Programs ---

    // Function to compile a shader and check for errors
    auto compileShader = [](GLenum type, const std::string& source) -> GLuint {
        GLuint shader = glCreateShader(type);
        const GLchar* src = source.c_str();
        glShaderSource(shader, 1, &src, NULL);
        glCompileShader(shader);

        // Check compilation status
        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            GLchar infoLog[512];
            glGetShaderInfoLog(shader, 512, NULL, infoLog);
            std::string shaderType = (type == GL_VERTEX_SHADER) ? "VERTEX" : "FRAGMENT";
            std::cerr << "ERROR::SHADER::" << shaderType << "::COMPILATION_FAILED\n" << infoLog << std::endl;
        }

        return shader;
    };

    // Function to link shaders into a program and check for errors
    auto createShaderProgram = [&](const std::string& vertexSrc, const std::string& fragmentSrc) -> GLuint {
        GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSrc);
        GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSrc);

        GLuint program = glCreateProgram();
        glAttachShader(program, vertexShader);
        glAttachShader(program, fragmentShader);
        glLinkProgram(program);

        // Check linking status
        GLint success;
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) {
            GLchar infoLog[512];
            glGetProgramInfoLog(program, 512, NULL, infoLog);
            std::cerr << "ERROR::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
        }

        // Shaders can be deleted after linking
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        return program;
    };

    // Create shader programs
    GLuint opaqueShaderProgram = createShaderProgram(opaqueVertexShaderSource, opaqueFragmentShaderSource);
    GLuint transparentShaderProgram = createShaderProgram(opaqueVertexShaderSource, transparentFragmentShaderSource);

    // Get uniform locations for view and projection matrices (common for both shader programs)
    GLint opaque_viewLoc = glGetUniformLocation(opaqueShaderProgram, "view");
    GLint opaque_projectionLoc = glGetUniformLocation(opaqueShaderProgram, "projection");

    GLint transparent_viewLoc = glGetUniformLocation(transparentShaderProgram, "view");
    GLint transparent_projectionLoc = glGetUniformLocation(transparentShaderProgram, "projection");

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

    // Yaw rotation variables
    float yaw = -90.0f; // Facing negative z by default
    float pitch = 0.0f; // Looking straight ahead
    const float yawSpeed = 60.0f; // Degrees per second

    // Compute cameraFront from yaw/pitch
    auto updateCameraFront = [&](float yaw, float pitch) -> glm::vec3 {
        glm::vec3 front;
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        return glm::normalize(front);
    };
    glm::vec3 cameraFront = updateCameraFront(yaw, pitch);

    // Initialize cameraRight and cameraUpAdjusted
    glm::vec3 cameraRight = glm::normalize(glm::cross(cameraFront, cameraUp));
    glm::vec3 cameraUpAdjusted = glm::normalize(glm::cross(cameraRight, cameraFront));

    // Camera up adjustment toggle
    bool enableCameraUpAdjusted = false;

    // Mouse look toggle
    bool enableMouseLook = false;
    window.setMouseCursorVisible(true);

    // Mouse center
    sf::Vector2i windowCenter(window.getSize().x / 2, window.getSize().y / 2);

    // --- Projection Matrix ---
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), 800.f / 600.f, 0.1f, 100.0f);

    // --- OpenGL State Setup ---
    // Enable depth test and multisampling
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE); // Enable multisampling for anti-aliasing

    // Note: GL_LINE_SMOOTH is removed to prevent GL_INVALID_VALUE errors in Core Profile

    // Set line width to 1.0f to prevent GL_INVALID_VALUE
    glLineWidth(1.0f); // Safe default in Core Profile

    // Initialize delta clock
    sf::Clock deltaClock;

    // Define movement speed (units per second)
    float velocity = 5.0f;

    // --- YAML Config Loading ---
    YAML::Node config;
    try {
        config = YAML::LoadFile("config.yaml");
    } catch (const YAML::Exception& e) {
        std::cerr << "Error loading config.yaml: " << e.what() << std::endl;
        return -1;
    }

    // Create Object3D instances
    std::vector<Object3D*> objects;
    for (const auto& objectConfig : config["objects"]) {
        objects.push_back(new Object3D(objectConfig));
    }

    while (window.isOpen()) {
        // --- Calculate delta time ---
        float deltaTime = deltaClock.restart().asSeconds();
        if (deltaTime > 0.1f) deltaTime = 0.1f; // Prevent large jumps

        // --- Event Handling ---
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            }

            // Toggle mouse look with 'M' key
            if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::M) {
                enableMouseLook = !enableMouseLook;
                if (enableMouseLook) {
                    window.setMouseCursorVisible(false);
                    sf::Mouse::setPosition(windowCenter, window);
                } else {
                    window.setMouseCursorVisible(true);
                }
            }

            // Toggle camera up adjustment with 'U' key
            if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::U) {
                enableCameraUpAdjusted = !enableCameraUpAdjusted;
                std::cout << "Camera Up Adjusted: " << (enableCameraUpAdjusted ? "Enabled" : "Disabled") << std::endl;
            }

            // Handle quitting via 'Escape' key
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

        // Handle yaw input
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::E)) {
            yaw += yawSpeed * deltaTime; // Rotate left
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Q)) {
            yaw -= yawSpeed * deltaTime; // Rotate right
        }

        // Optional: Clamp yaw to [0, 360) degrees to prevent overflow
        if (yaw >= 360.0f || yaw <= -360.0f) {
            yaw = 0.0f;
        }

        // Apply movement with normalization and deltaTime
        if (glm::length(movement) != 0.0f) {
            movement = glm::normalize(movement) * velocity * deltaTime;
            cameraPos += movement;
        }

        // --- Camera Orientation Update ---
        cameraFront = updateCameraFront(yaw, pitch);
        cameraRight = glm::normalize(glm::cross(cameraFront, cameraUp));

        // Update cameraUpAdjusted based on yaw
        cameraUpAdjusted = glm::normalize(glm::cross(cameraRight, cameraFront));

        // --- Transformations ---
        // Update view matrix
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront,
                                     (enableCameraUpAdjusted ? cameraUpAdjusted : cameraUp));

        // --- Drawing ---
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f); // White background
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // --- Draw Objects ---
        for (const auto& object : objects) {
            object->draw(opaqueShaderProgram, transparentShaderProgram, view, projection);
        }

        // --- Draw Grid ---
        // Use the opaque shader for the grid (fully opaque)
        glUseProgram(opaqueShaderProgram);

        // Set uniforms
        GLint modelLoc = glGetUniformLocation(opaqueShaderProgram, "model");
        GLint viewLoc = glGetUniformLocation(opaqueShaderProgram, "view");
        GLint projectionLoc = glGetUniformLocation(opaqueShaderProgram, "projection");

        glm::mat4 gridModel = glm::mat4(1.0f);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(gridModel));
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));

        // Disable blending for grid rendering
        glDisable(GL_BLEND);

        // Bind grid VAO and draw lines
        glBindVertexArray(gridVAO);
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(gridVertices.size() / 6)); // each vertex has 6 floats (pos + color)
        checkOpenGLError("Draw Grid");
        glBindVertexArray(0);

        window.display();
    }

    // --- Cleanup ---
    // Delete objects (this will call their destructors and clean up OpenGL resources)
    for (const auto& object : objects) {
        delete object;
    }

    // Delete grid resources
    glDeleteVertexArrays(1, &gridVAO);
    glDeleteBuffers(1, &gridVBO);

    // Delete shader programs
    glDeleteProgram(opaqueShaderProgram);
    glDeleteProgram(transparentShaderProgram);

    return 0;
}
