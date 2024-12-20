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

int main() {
    // --- SFML Window Setup ---
    sf::ContextSettings settings;
    settings.depthBits = 24;
    settings.stencilBits = 8;
    settings.majorVersion = 3;
    settings.minorVersion = 3;
    settings.antialiasingLevel = 16;
    settings.attributeFlags = sf::ContextSettings::Core; // Core profile

    sf::RenderWindow window(sf::VideoMode(800, 600), "3D Engine with SFML - Multicolored Cube", sf::Style::Default, settings);
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

    std::cout << "Model uniform location: " << modelLoc << std::endl;
    std::cout << "View uniform location: " << viewLoc << std::endl;
    std::cout << "Projection uniform location: " << projectionLoc << std::endl;

    // --- Cube Data ---
    // We'll store positions and colors interleaved: (x, y, z, r, g, b) per vertex
    float vertices[] = {
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

    unsigned int indices[] = {
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

    // Create VAO
    GLuint VAO;
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    checkOpenGLError("Bind Vertex Array");

    // Create VBO
    GLuint VBO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    checkOpenGLError("Bind VBO");

    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    checkOpenGLError("Upload VBO data");

    // Create EBO
    GLuint EBO;
    glGenBuffers(1, &EBO);
    checkOpenGLError("Create EBO");

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    checkOpenGLError("Bind EBO");

    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    checkOpenGLError("Upload EBO data");

    // Vertex attribute pointer for position (location = 0)
    // Each vertex: 3 floats position, 3 floats color, total stride = 6 * sizeof(float)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    checkOpenGLError("Set vertex attribute pointer for position");
    glEnableVertexAttribArray(0);
    checkOpenGLError("Enable vertex attribute array for position");

    // Vertex attribute pointer for color (location = 1)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    checkOpenGLError("Set vertex attribute pointer for color");
    glEnableVertexAttribArray(1);
    checkOpenGLError("Enable vertex attribute array for color");

    glBindVertexArray(0);

    // --- Camera Setup ---
    glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
    glm::vec3 cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

    // --- Projection Matrix ---
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 100.0f);

    // Enable depth test
    glEnable(GL_DEPTH_TEST);

    // --- Main Loop ---
    sf::Clock clock; // to rotate the cube over time
    while (window.isOpen()) {
        // --- Event Handling ---
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            }

            // Keyboard input for camera movement
            if (event.type == sf::Event::KeyPressed) {
                if (event.key.code == sf::Keyboard::Left) {
                    cameraPos.x -= 0.1f;
                } else if (event.key.code == sf::Keyboard::Right) {
                    cameraPos.x += 0.1f;
                } else if (event.key.code == sf::Keyboard::Down) {
                    cameraPos.y -= 0.1f;
                } else if (event.key.code == sf::Keyboard::Up) {
                    cameraPos.y += 0.1f;
                } else if (event.key.code == sf::Keyboard::W) {
                    cameraPos.z -= 0.1f;
                } else if (event.key.code == sf::Keyboard::S) {
                    cameraPos.z += 0.1f;
                }
            }
        }

        // --- Transformations ---
        glm::mat4 model = glm::mat4(1.0f);
        // Move the cube back
        model = glm::translate(model, glm::vec3(0.0f, 0.0f, -5.0f));
        // Rotate the cube over time
        float angle = clock.getElapsedTime().asSeconds();
        // model = glm::rotate(model, angle * glm::radians(20.0f), glm::vec3(1.0f, 1.0f, 0.0f));

        glm::mat4 view = glm::lookAt(cameraPos, cameraTarget, cameraUp);

        // --- Drawing ---
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f); // White background
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Use the shader program
        glUseProgram(shaderProgram);

        // Set uniform values
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));

        // Bind VAO and draw the cube
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        checkOpenGLError("Draw");

        glBindVertexArray(0);
        window.display();
    }

    // --- Cleanup ---
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteProgram(shaderProgram);

    return 0;
}
