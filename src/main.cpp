#ifdef __APPLE__
// Use the Core OpenGL headers on macOS
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
#include <glm/gtx/string_cast.hpp> // For glm::to_string

#include <iostream>
#include <vector>

// Shader sources
const std::string vertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;

    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;

    void main()
    {
        gl_Position = projection * view * model * vec4(aPos, 1.0);
    }
)";

const std::string fragmentShaderSource = R"(
    #version 330 core
    out vec4 FragColor;

    void main()
    {
        FragColor = vec4(1.0, 1.0, 1.0, 1.0); // White color
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

int main() {
    // --- SFML Window Setup ---
    sf::ContextSettings settings;
    settings.depthBits = 24;
    settings.stencilBits = 8;
    settings.majorVersion = 3;
    settings.minorVersion = 3;
    settings.antialiasingLevel = 8;
    settings.attributeFlags = sf::ContextSettings::Core; // Request core profile

    sf::RenderWindow window(sf::VideoMode(800, 600), "3D Engine with SFML", sf::Style::Default, settings);
    window.setVerticalSyncEnabled(true);
    window.setFramerateLimit(60);
    window.setActive(true);

#ifndef __APPLE__
    // Initialize GLEW on non-Apple platforms. On Apple, gl3.h is enough for core GL.
    glewExperimental = GL_TRUE;
    GLenum glewStatus = glewInit();
    if (glewStatus != GLEW_OK) {
        std::cerr << "Error initializing GLEW: " << glewGetErrorString(glewStatus) << std::endl;
        return -1;
    }
#endif

    // --- Shader Program ---
    // 1. Create and compile the vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    const GLchar* vss = vertexShaderSource.c_str();
    glShaderSource(vertexShader, 1, &vss, NULL);
    glCompileShader(vertexShader);
    checkOpenGLError("Compile Vertex Shader");

    // Check for vertex shader compilation errors
    GLint success;
    GLchar infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    // 2. Create and compile the fragment shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    const GLchar* fss = fragmentShaderSource.c_str();
    glShaderSource(fragmentShader, 1, &fss, NULL);
    glCompileShader(fragmentShader);
    checkOpenGLError("Compile Fragment Shader");

    // Check for fragment shader compilation errors
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    // 3. Link the shaders into a shader program
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    checkOpenGLError("Link Shader Program");

    // Check for linking errors
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }

    // Delete the shaders (they are now linked into the program)
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Check uniform locations
    GLint modelLoc = glGetUniformLocation(shaderProgram, "model");
    GLint viewLoc = glGetUniformLocation(shaderProgram, "view");
    GLint projectionLoc = glGetUniformLocation(shaderProgram, "projection");
    std::cout << "Model uniform location: " << modelLoc << std::endl;
    std::cout << "View uniform location: " << viewLoc << std::endl;
    std::cout << "Projection uniform location: " << projectionLoc << std::endl;

    // --- 3D Data (Quad) ---
    std::vector<float> vertices = {
        // Positions
        -0.5f, -0.5f, 0.0f,  // Bottom left
         0.5f, -0.5f, 0.0f,  // Bottom right
         0.5f,  0.5f, 0.0f,  // Top right
        -0.5f,  0.5f, 0.0f   // Top left
    };

    std::vector<unsigned int> indices = {
        0, 1, 2,  // First triangle
        0, 2, 3   // Second triangle
    };

    std::cout << "Vertices: ";
    for (float v : vertices) {
        std::cout << v << " ";
    }
    std::cout << std::endl;

    std::cout << "Indices: ";
    for (unsigned int i : indices) {
        std::cout << i << " ";
    }
    std::cout << std::endl;

    // Create and bind a VAO
    GLuint VAO;
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    checkOpenGLError("Bind Vertex Array");

    // Create and bind a VBO
    GLuint VBO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    checkOpenGLError("Bind VBO");

    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    checkOpenGLError("Upload VBO data");

    // Verify data in VBO
    std::vector<float> uploadedVertices(vertices.size());
    glGetBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(float), uploadedVertices.data());
    checkOpenGLError("Read VBO data");
    std::cout << "Uploaded Vertices: ";
    for (float v : uploadedVertices) {
        std::cout << v << " ";
    }
    std::cout << std::endl;

    // Create and bind an EBO
    GLuint EBO;
    glGenBuffers(1, &EBO);
    checkOpenGLError("Create EBO");

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    checkOpenGLError("Bind EBO");

    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
    checkOpenGLError("Upload EBO data");

    // Verify data in EBO
    std::vector<unsigned int> uploadedIndices(indices.size());
    glGetBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, indices.size() * sizeof(unsigned int), uploadedIndices.data());
    checkOpenGLError("Read EBO data");
    std::cout << "Uploaded Indices: ";
    for (unsigned int i : uploadedIndices) {
        std::cout << i << " ";
    }
    std::cout << std::endl;

    // Set vertex attribute pointers
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    checkOpenGLError("Set vertex attribute pointer");

    glEnableVertexAttribArray(0);
    checkOpenGLError("Enable vertex attribute array");

    // Unbind VAO
    glBindVertexArray(0);

    // --- Camera Setup ---
    glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
    glm::vec3 cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

    // --- Projection Matrix ---
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 100.0f);

    // Enable depth test once (not strictly necessary every frame)
    glEnable(GL_DEPTH_TEST);

    // --- Main Loop ---
    while (window.isOpen()) {
        // --- Event Handling ---
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            }
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
        model = glm::translate(model, glm::vec3(0.0f, 0.0f, -5.0f)); // Move the quad back
        glm::mat4 view = glm::lookAt(cameraPos, cameraTarget, cameraUp);

        // Print matrices
        std::cout << "Model Matrix:\n" << glm::to_string(model) << std::endl;
        std::cout << "View Matrix:\n" << glm::to_string(view) << std::endl;
        std::cout << "Projection Matrix:\n" << glm::to_string(projection) << std::endl;

        // --- Drawing ---
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Use the shader program
        glUseProgram(shaderProgram);

        // Set uniform values in the shader
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));

        // Bind the VAO
        glBindVertexArray(VAO);

        // Draw the quad using glDrawElements
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        checkOpenGLError("Draw");

        // Unbind VAO
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
