// UAM Simulator — modularized main with comments and clearer structure

#include <GL/glew.h>
#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ===================== Math / Pose =====================
struct Pose {
    glm::vec3 pos{0.f, 1.0f, 0.f};
    float yaw = 0.f;   // degrees (CCW around +Y when seen from above)
    float pitch = 0.f; // degrees
};

// Forward from yaw/pitch: yaw=0 -> +X, yaw=90 -> +Z
static inline glm::vec3 forwardFrom(const Pose& p) {
    float y = glm::radians(p.yaw), pit = glm::radians(p.pitch);
    return glm::normalize(glm::vec3(std::cos(y) * std::cos(pit),
                                    std::sin(pit),
                                    std::sin(y) * std::cos(pit)));
}

// ===================== Shader utils =====================
namespace glutil {

// Load text file into string (used for shader source)
static std::string loadText(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "ERROR: Could not open file: " << filepath << std::endl;
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Compile + link shader program from vertex/fragment paths
static GLuint createShaderProgram(const std::string& vertPath, const std::string& fragPath) {
    std::string vertSource = loadText(vertPath);
    std::string fragSource = loadText(fragPath);
    if (vertSource.empty() || fragSource.empty()) return 0;

    const char* vsrc = vertSource.c_str();
    const char* fsrc = fragSource.c_str();

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vsrc, nullptr);
    glCompileShader(vs);
    GLint ok = GL_FALSE;
    glGetShaderiv(vs, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024]; glGetShaderInfoLog(vs, 1024, nullptr, log);
        std::cerr << "Vertex shader compile error:\n" << log << std::endl;
    }

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fsrc, nullptr);
    glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024]; glGetShaderInfoLog(fs, 1024, nullptr, log);
        std::cerr << "Fragment shader compile error:\n" << log << std::endl;
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024]; glGetProgramInfoLog(prog, 1024, nullptr, log);
        std::cerr << "Program link error:\n" << log << std::endl;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    if (ok) std::cout << "Shader program loaded: " << vertPath << " | " << fragPath << std::endl;
    return prog;
}

} // namespace glutil

// ===================== Drone =====================
struct Drone {
    Pose p;
    float speed = 6.f;    // m/s
    float yawRate = 90.f; // deg/s
    float climb = 3.f;    // m/s
    glm::vec3 bodyScale{0.5f, 0.15f, 0.5f};
};

// Read keyboard and update drone physics
static inline void updateDrone(Drone& d, float dt) {
    glm::vec3 fwd = forwardFrom(d.p);
    glm::vec3 right = glm::normalize(glm::cross(fwd, {0, 1, 0}));

    glm::vec3 move(0);
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) move += fwd;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) move -= fwd;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) move -= right;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) move += right;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::C)) d.p.pos.y += d.climb * dt;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::V)) d.p.pos.y -= d.climb * dt;

    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Q)) d.p.yaw -= d.yawRate * dt;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::E)) d.p.yaw += d.yawRate * dt;

    if (glm::length(move) > 0.f) d.p.pos += glm::normalize(move) * (d.speed * dt);
}

// Draw drone as a scaled cube using object shader
static inline void drawDroneBox(const Pose& p, const glm::vec3& scale, GLuint shaderProgram, GLuint vao) {
    glm::mat4 model(1.f);
    model = glm::translate(model, p.pos);
    model = glm::rotate(model, glm::radians(-p.yaw), glm::vec3(0, 1, 0));
    model = glm::rotate(model, glm::radians(p.pitch), glm::vec3(1, 0, 0));
    model = glm::scale(model, scale);

    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(model));
    glUniform3f(glGetUniformLocation(shaderProgram, "uObjectColor"), 0.15f, 0.15f, 0.15f);

    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

void drawDroneProjectedShadow(const Drone& drone, const glm::vec3& lightPos, GLuint shadowShader, GLuint cubeVAO, float groundY = 0.0f) {
    glUseProgram(shadowShader);
    
    // Lichtposition (L)
    float Lx = lightPos.x;
    float Ly = lightPos.y;
    float Lz = lightPos.z;

    // Ebene (Y=groundY)
    float A = 0.0f;
    float B = 1.0f;
    float C = 0.0f;
    float D = -groundY - 0.001f; // Bias

    // Dot product: L.P (Homogener Punkt L mit Ebene P) = Lx*A + Ly*B + Lz*C + 1*D
    float dot = Lx * A + Ly * B + Lz * C + 1.0f * D;

    // Planar Projection Matrix M
    glm::mat4 M(1.0f);
    
    // M_ij = (Dot) * delta_ij - L_i * P_j

    // Spalte 0 (x)
    M[0][0] = dot - Lx * A;
    M[1][0] = 0.0f - Lx * B;
    M[2][0] = 0.0f - Lx * C;
    M[3][0] = 0.0f - Lx * D;

    // Spalte 1 (y)
    M[0][1] = 0.0f - Ly * A;
    M[1][1] = dot - Ly * B;
    M[2][1] = 0.0f - Ly * C;
    M[3][1] = 0.0f - Ly * D;

    // Spalte 2 (z)
    M[0][2] = 0.0f - Lz * A;
    M[1][2] = 0.0f - Lz * B;
    M[2][2] = dot - Lz * C;
    M[3][2] = 0.0f - Lz * D;

    // Spalte 3 (w)
    M[0][3] = 0.0f - 1.0f * A;
    M[1][3] = 0.0f - 1.0f * B;
    M[2][3] = 0.0f - 1.0f * C;
    M[3][3] = dot - 1.0f * D; 

    // Drohnen-Modell-Matrix
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, drone.p.pos);
    glm::vec3 shadowScale(drone.bodyScale.x * 2.0f, drone.bodyScale.y, drone.bodyScale.z * 2.0f);
    model = glm::scale(model, shadowScale);
    
    // Schattenmodell-Matrix = Projektionsmatrix * Drohnen-Modell-Matrix
    glm::mat4 shadowModel = M * model;

    GLint modelLoc = glGetUniformLocation(shadowShader, "model");
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(shadowModel));

    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    glUseProgram(0);
}

// ===================== Scene Objects (Buildings) =====================
class Object3D {
public:
    std::string name;
    glm::vec3 position;
    glm::vec3 dimensions;
    glm::vec3 color;
    glm::vec3 scale{1.0f, 1.0f, 1.0f};

    // Construct from YAML node
    explicit Object3D(const YAML::Node& config) {
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

    glm::mat4 getModelMatrix() const {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, position);
        model = glm::scale(model, scale);
        return model;
    }

    // Draw a cube for this object using provided shader/VAO
    void draw(GLuint shaderProgram, GLuint vao) const {
        glm::mat4 model(1.f);
        model = glm::translate(model, position);
        model = glm::scale(model, dimensions);

        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(model));
        if (glGetUniformLocation(shaderProgram, "uObjectColor") != -1) {
            glUniform3fv(glGetUniformLocation(shaderProgram, "uObjectColor"), 1, glm::value_ptr(color));
        }

        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
    }

    static void drawProjectedShadow(const Object3D& object, const glm::vec3& lightPos, GLuint shadowShader, GLuint cubeVAO, float groundY = 0.0f) {
        glUseProgram(shadowShader);
        
        // Lichtposition (L)
        float Lx = lightPos.x;
        float Ly = lightPos.y;
        float Lz = lightPos.z;

        // Ebene (Y=groundY)
        // Normal N=(0, 1, 0) -> A=0, B=1, C=0
        // D (Abstand zum Ursprung) ist -groundY
        float A = 0.0f;
        float B = 1.0f;
        float C = 0.0f;
        // Bias: Verschiebt die Ebene leicht nach oben (Z-Fighting vermeiden)
        float D = -groundY - 0.001f; 

        // Dot product: L.P (Homogener Punkt L mit Ebene P) = Lx*A + Ly*B + Lz*C + 1*D
        float dot = Lx * A + Ly * B + Lz * C + 1.0f * D;

        // Planar Projection Matrix M (Standardformel)
        glm::mat4 M(1.0f);

        // M_ij = (Dot) * delta_ij - L_i * P_j
        
        // Spalte 0 (x)
        M[0][0] = dot - Lx * A;
        M[1][0] = 0.0f - Lx * B;
        M[2][0] = 0.0f - Lx * C;
        M[3][0] = 0.0f - Lx * D;

        // Spalte 1 (y)
        M[0][1] = 0.0f - Ly * A;
        M[1][1] = dot - Ly * B;
        M[2][1] = 0.0f - Ly * C;
        M[3][1] = 0.0f - Ly * D;

        // Spalte 2 (z)
        M[0][2] = 0.0f - Lz * A;
        M[1][2] = 0.0f - Lz * B;
        M[2][2] = dot - Lz * C;
        M[3][2] = 0.0f - Lz * D;

        // Spalte 3 (w)
        M[0][3] = 0.0f - 1.0f * A;
        M[1][3] = 0.0f - 1.0f * B;
        M[2][3] = 0.0f - 1.0f * C;
        M[3][3] = dot - 1.0f * D; 

        // Schattenmodell-Matrix = Projektionsmatrix * Objekt-Modell-Matrix
        glm::mat4 shadowModel = M * object.getModelMatrix();

        GLint modelLoc = glGetUniformLocation(shadowShader, "model");
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(shadowModel));

        glBindVertexArray(cubeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
        glUseProgram(0);
    }
};

// ===================== Grid helpers =====================
// Create colored grid vertices around origin (XZ plane)
static std::vector<float> generateGridVertices(int halfSize = 10) {
    std::vector<float> vertices;
    float lineColor[3] = {0.7f, 0.7f, 0.7f};
    float centerColor[3] = {0.3f, 0.3f, 0.3f};

    float yOffset = 0.001f; // Slightly above ground to avoid Z-fighting

    for (int x = -halfSize; x <= halfSize; x++) {
        bool isCenter = (x == 0);
        auto [r, g, b] = std::tuple{isCenter ? centerColor[0] : lineColor[0],
                                    isCenter ? centerColor[1] : lineColor[1],
                                    isCenter ? centerColor[2] : lineColor[2]};
        vertices.insert(vertices.end(), {
            static_cast<float>(x), yOffset, static_cast<float>(-halfSize), r, g, b,
            static_cast<float>(x), yOffset, static_cast<float>(halfSize),  r, g, b
        });
    }
    for (int z = -halfSize; z <= halfSize; z++) {
        bool isCenter = (z == 0);
        auto [r, g, b] = std::tuple{isCenter ? centerColor[0] : lineColor[0],
                                    isCenter ? centerColor[1] : lineColor[1],
                                    isCenter ? centerColor[2] : lineColor[2]};
        vertices.insert(vertices.end(), {
            static_cast<float>(-halfSize), yOffset, static_cast<float>(z), r, g, b,
            static_cast<float>(halfSize),  yOffset, static_cast<float>(z), r, g, b
        });
    }
    return vertices;
}

// Create VAO/VBO for grid lines
static void createGridVAO(GLuint& vao, GLuint& vbo, int& vertexCount) {
    std::vector<float> gridVertices = generateGridVertices(10);
    vertexCount = static_cast<int>(gridVertices.size() / 6);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, gridVertices.size() * sizeof(float), gridVertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

// Create VAO/VBO for ground plane
static void createGroundVAO(GLuint& vao, GLuint& vbo) {
        float groundVertices[] = {
        // PosX, PosY, PosZ, NormalX, NormalY, NormalZ (6 floats pro Vertex)
        -100.0f, 0.0f, -100.0f, 0.0f, 1.0f, 0.0f, // Unten links
        100.0f, 0.0f, -100.0f, 0.0f, 1.0f, 0.0f, // Unten rechts
        -100.0f, 0.0f,  100.0f, 0.0f, 1.0f, 0.0f, // Oben links

        100.0f, 0.0f, -100.0f, 0.0f, 1.0f, 0.0f, // Unten rechts
        100.0f, 0.0f,  100.0f, 0.0f, 1.0f, 0.0f, // Oben rechts
        -100.0f, 0.0f,  100.0f, 0.0f, 1.0f, 0.0f  // Oben links
    };

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(groundVertices), groundVertices, GL_STATIC_DRAW);

    // Position Attribute (Location 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Normale Attribute (Location 1) - Wichtig für den Schatten-Bias!
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

// ===================== Cube mesh (for buildings/drone) =====================
static void createCubeVAO(GLuint& vao, GLuint& vbo) {
    // Interleaved (pos, normal). 36 vertices.
    float cubeVertices[] = {
        // Rückseite (-Z)
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,

        // Vorderseite (+Z)
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
        0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
        0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
        0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,

        // Links (-X)
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,

        // Rechts (+X)
        0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
        0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
        0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
        0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
        0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
        0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,

        // Unten (-Y)
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
        0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
        0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
        0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,

        // Oben (+Y)
        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
        0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
        0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
        0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f
    };

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);

    GLsizei stride = 6 * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

static void createSensorVAO(GLuint& vao, GLuint& vbo) {

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    // VBO mit dynamischem Speicher reservieren (3 Floats pro Punkt/Vertex)
    glBufferData(GL_ARRAY_BUFFER, 10000 * 3 * sizeof(float), NULL, GL_DYNAMIC_DRAW); 
    // Position (location 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// ===================== Simple light/geometry helpers =====================
static inline bool projectPointToGround(const glm::vec3& L, const glm::vec3& P, float yPlane, glm::vec3& out) {
    glm::vec3 dir = P - L;
    if (std::abs(dir.y) < 1e-6f) return false;
    float t = (yPlane - L.y) / dir.y;
    if (t <= 0.f) return false;
    out = L + t * dir;
    return true;
}
static std::vector<glm::vec3> convexHullXZ(std::vector<glm::vec3> pts); // forward

// ===================== Lidar / Radar (simulation + debug draw) =====================
struct AABB { glm::vec3 mn, mx; int id; };
static inline AABB makeAABB(const Object3D& o) {
    glm::vec3 h = 0.5f * o.dimensions;
    return {o.position - h, o.position + h, 0};
}

static inline bool rayAABB(const glm::vec3& ro, const glm::vec3& rd, const AABB& b, float tMax, float& tHit) {
    float tmin = 0.001f, tmax = tMax;
    for (int i=0;i<3;++i){
        float d = rd[i]; if (std::abs(d) < 1e-6f) d = (d<0 ? -1e-6f : 1e-6f);
        float invD = 1.f / d;
        float t0 = (b.mn[i] - ro[i]) * invD;
        float t1 = (b.mx[i] - ro[i]) * invD;
        if (invD < 0.f) std::swap(t0, t1);
        tmin = t0 > tmin ? t0 : tmin;
        tmax = t1 < tmax ? t1 : tmax;
        if (tmax <= tmin) return false;
    }
    tHit = tmin;
    return true;
}

struct Hit3D { bool ok; float range; glm::vec3 point; int objId; glm::vec3 dir; };
static inline std::vector<Hit3D> simulateLidar3D(const Pose& p,
                                                 const std::vector<AABB>& world,
                                                 int beamsH, int beamsV,
                                                 float fovH_deg, float fovV_deg,
                                                 float maxR) {
    std::vector<Hit3D> hits;
    hits.reserve(beamsH * beamsV);

    // Basis from DRONE pose (not camera!)
    float yaw=glm::radians(p.yaw), pit=glm::radians(p.pitch);
    glm::vec3 F = glm::normalize(glm::vec3(std::cos(yaw)*std::cos(pit), std::sin(pit), std::sin(yaw)*std::cos(pit)));
    glm::vec3 R = glm::normalize(glm::cross(F, {0,1,0}));
    glm::vec3 U = glm::normalize(glm::cross(R, F));

    for (int j=0; j<beamsV; ++j) {
        float v = ((j/(float)(beamsV-1)) - 0.5f) * glm::radians(fovV_deg);
        for (int i=0; i<beamsH; ++i) {
            float h = ((i/(float)(beamsH-1)) - 0.5f) * glm::radians(fovH_deg);
            glm::vec3 dir = glm::normalize(F + std::tan(h)*R + std::tan(v)*U);

            float best = maxR; int bestId=-1;
            for (const auto& b : world){
                float th; if (rayAABB(p.pos, dir, b, maxR, th)) {
                    if (th < best) { best = th; bestId = b.id; }
                }
            }
            if (bestId>=0) hits.push_back({true, best, p.pos + dir*best, bestId, dir});
            else           hits.push_back({false, maxR, p.pos + dir*maxR, -1, dir});
        }
    }
    return hits;
}

struct RadarParams {
    int beamsH = 60, beamsV = 8;
    float fovH = 90.f, fovV = 20.f;
    float maxR = 150.f, minR = 2.f;
    float snr0 = 40.f, snrMin = 8.f;
};
struct RadarDet { bool ok; float range, vr, az, el; glm::vec3 point; int objId; };

static inline std::vector<RadarDet> simulateRadar3D(const Pose& sensorPose,
                                                    const glm::vec3& sensorVel,
                                                    const std::vector<AABB>& world,
                                                    const RadarParams& R) {
    std::vector<RadarDet> dets;
    dets.reserve(R.beamsH * R.beamsV);

    float yaw = glm::radians(sensorPose.yaw), pit = glm::radians(sensorPose.pitch);
    glm::vec3 F = glm::normalize(glm::vec3(std::cos(yaw)*std::cos(pit), std::sin(pit), std::sin(yaw)*std::cos(pit)));
    glm::vec3 Rv = glm::normalize(glm::cross(F, glm::vec3(0,1,0)));
    glm::vec3 U  = glm::normalize(glm::cross(Rv, F));

    for (int v = 0; v < R.beamsV; ++v) {
        float el = ((v / (float)(R.beamsV-1)) - 0.5f) * glm::radians(R.fovV);
        for (int h = 0; h < R.beamsH; ++h) {
            float az = ((h / (float)(R.beamsH-1)) - 0.5f) * glm::radians(R.fovH);

            glm::vec3 dir = glm::normalize(F + std::tan(az)*Rv + std::tan(el)*U);

            float best = R.maxR; int bestId = -1;
            for (const auto& box : world) {
                float th;
                if (rayAABB(sensorPose.pos, dir, box, R.maxR, th)) {
                    if (th < best && th > R.minR) { best = th; bestId = box.id; }
                }
            }

            if (bestId < 0) {
                dets.push_back({false, R.maxR, 0.f, az, el, sensorPose.pos + dir*R.maxR, -1});
                continue;
            }

            // SNR ~ snr0 - 40 log10(R) (1/R^4 power law)
            float snr = R.snr0 - 40.f * std::log10(std::max(best, 1e-2f));
            if (snr < R.snrMin) {
                dets.push_back({false, R.maxR, 0.f, az, el, sensorPose.pos + dir*R.maxR, -1});
                continue;
            }

            // Radial velocity: projection of platform velocity onto LOS, + closing
            // float vr = glm::dot(sensorVel, dir) * (-1.0f); // flip to make +closing
            float vr = glm::dot(sensorVel, dir); // flip to make +closing

            dets.push_back({true, best, vr, az, el, sensorPose.pos + dir*best, bestId});
        }
    }
    return dets;
}

// Zeichnet einen Würfel an der Lichtposition (unbeleuchtet)
static inline void drawLightSource(const glm::vec3& lightPos, GLuint shaderProgram, GLuint vao, const glm::mat4& view, const glm::mat4& projection) {
    glUseProgram(shaderProgram);
    
    // Setze View/Projection Matrizen
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "uProjection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "uView"), 1, GL_FALSE, glm::value_ptr(view));
    
    // Model Matrix (Würfel auf Lichtposition skalieren/platzieren)
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, lightPos);
    model = glm::scale(model, glm::vec3(0.2f)); // Kleinere Licht-Kugel
    
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(model));
    glUniform3f(glGetUniformLocation(shaderProgram, "uColor"), 1.0f, 1.0f, 0.0f); // Gelbe Lichtfarbe

    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    glUseProgram(0);
}

// ===================== Draw lines and points helpers =====================
// Helfer zum Zeichnen von Punkten (LiDAR, Radar)
static void drawPoints(const std::vector<glm::vec3>& points, const glm::vec3& color, float pointSize, GLuint shaderProgram, GLuint vao, GLuint vbo, const glm::mat4& view, const glm::mat4& projection) {
    if (points.empty()) return;

    // 1. Daten in VBO übertragen (dynamisches Update)
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    // Ersetze den VBO-Inhalt jedes Frame
    glBufferData(GL_ARRAY_BUFFER, points.size() * sizeof(glm::vec3), points.data(), GL_DYNAMIC_DRAW); 
    
    // 2. Shader aktivieren
    glUseProgram(shaderProgram);

    // 3. Uniforms senden
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "uProjection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "uView"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f))); // Identitätsmatrix
    glUniform3fv(glGetUniformLocation(shaderProgram, "uColor"), 1, glm::value_ptr(color));
    glUniform1f(glGetUniformLocation(shaderProgram, "uPointSize"), pointSize);

    // 4. Zeichnen
    glEnable(GL_PROGRAM_POINT_SIZE); 
    glEnable(GL_DEPTH_TEST);
    glBindVertexArray(vao);
    glDrawArrays(GL_POINTS, 0, points.size());
    glBindVertexArray(0);
    glDisable(GL_PROGRAM_POINT_SIZE);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
}

// Helfer zum Zeichnen von Linien (Radar Beams, LiDAR Beams)
static void drawLines(const std::vector<glm::vec3>& vertices, const glm::vec3& color, float lineWidth, GLuint shaderProgram, GLuint vao, GLuint vbo, const glm::mat4& view, const glm::mat4& projection) {
    if (vertices.empty()) return;

    // 1. Daten in VBO übertragen (dynamisches Update)
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), vertices.data(), GL_DYNAMIC_DRAW); 
    
    // 2. Shader aktivieren
    glUseProgram(shaderProgram);

    // 3. Uniforms senden
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "uProjection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "uView"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f))); // Identitätsmatrix
    glUniform3fv(glGetUniformLocation(shaderProgram, "uColor"), 1, glm::value_ptr(color));
    
    glLineWidth(lineWidth);

    // 4. Zeichnen
    glEnable(GL_DEPTH_TEST);
    glBindVertexArray(vao);
    glDrawArrays(GL_LINES, 0, vertices.size());
    glBindVertexArray(0);
    
    glLineWidth(1.0f); // Zurücksetzen
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
}

// ===================== Shadow map (depth-only FBO) =====================
struct ShadowMap {
    GLuint fbo = 0;
    GLuint depthTex = 0;
    unsigned w = 4096, h = 4096;

    void init() {
        glGenFramebuffers(1, &fbo);
        glGenTextures(1, &depthTex);

        glBindTexture(GL_TEXTURE_2D, depthTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, w, h, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        float border[] = {1, 1, 1, 1};
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTex, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "ERROR::FRAMEBUFFER:: Shadow Map FBO incomplete\n";
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void bindForWrite() const {
        glViewport(0, 0, w, h);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glClear(GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
    }

    void bindForRead(GLuint shader, GLint samplerUniform = 0) const {
        glActiveTexture(GL_TEXTURE0 + samplerUniform);
        glBindTexture(GL_TEXTURE_2D, depthTex);
        glUniform1i(glGetUniformLocation(shader, "uShadowMap"), samplerUniform);
    }
};

// ===================== Camera state/update =====================
struct Camera {
    glm::vec3 pos{0.0f, 3.0f, 8.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    glm::vec3 front{0.0f, 0.0f, -1.0f};
    glm::vec3 right{1.0f, 0.0f, 0.0f};
    float yaw = -90.f, pitch = -10.f;

    glm::vec3 calcFront(float yaw_, float pitch_) const {
        glm::vec3 f;
        f.x = std::cos(glm::radians(yaw_)) * std::cos(glm::radians(pitch_));
        f.y = std::sin(glm::radians(pitch_));
        f.z = std::sin(glm::radians(yaw_)) * std::cos(glm::radians(pitch_));
        return glm::normalize(f);
    }
};

// Apply free-camera movement (WASD + CV for up/down)
static void updateFreeCamera(Camera& cam, float dt, float speed) {
    glm::vec3 move(0.0f);
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) move += cam.front;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) move -= cam.front;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) move -= cam.right;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) move += cam.right;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::C)) move += cam.up;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::V)) move -= cam.up;

    if (glm::length(move) > 0.0f) cam.pos += glm::normalize(move) * (speed * dt);
}

// Place follow-camera behind/above drone and look forward
static void updateFollowCamera(const Drone& drone, Camera& cam) {
    const float back = 4.0f, up = 2.0f, lead = 2.0f;

    float y = glm::radians(drone.p.yaw);
    glm::vec3 F_cam = glm::normalize(glm::vec3(std::cos(y), 0.0f, std::sin(y)));

    cam.pos = drone.p.pos - F_cam * back + cam.up * up;
    glm::vec3 target = drone.p.pos + F_cam * lead;

    cam.front = glm::normalize(target - cam.pos);
    cam.right = glm::normalize(glm::cross(cam.front, cam.up));
}

// ===================== Scene load helpers =====================
// Load objects from config.yaml; if missing, create defaults
static std::vector<std::unique_ptr<Object3D>> loadObjectsOrDefault(const std::string& cfgPath) {
    std::vector<std::unique_ptr<Object3D>> objects;
    try {
        YAML::Node config = YAML::LoadFile(cfgPath);
        for (const auto& objectConfig : config["objects"]) {
            objects.push_back(std::make_unique<Object3D>(objectConfig));
        }
        std::cout << "Loaded " << objects.size() << " objects from " << cfgPath << "\n";
    } catch (...) {
        std::cerr << "Error loading " << cfgPath << ", creating default objects...\n";
        YAML::Node o1; o1["name"]="RedCube";   o1["position"]=std::vector<float>{0,1,0};  o1["dimensions"]=std::vector<float>{1,1,1};   o1["color"]=std::vector<float>{1,0,0};
        YAML::Node o2; o2["name"]="GreenCube"; o2["position"]=std::vector<float>{3,1,2};  o2["dimensions"]=std::vector<float>{1,2,1};   o2["color"]=std::vector<float>{0,1,0};
        YAML::Node o3; o3["name"]="BlueCube";  o3["position"]=std::vector<float>{-2,0.5f,1}; o3["dimensions"]=std::vector<float>{1,0.5f,1}; o3["color"]=std::vector<float>{0,0,1};
        objects.push_back(std::make_unique<Object3D>(o1));
        objects.push_back(std::make_unique<Object3D>(o2));
        objects.push_back(std::make_unique<Object3D>(o3));
    }
    return objects;
}

// Build static AABB list for ray tests
static std::vector<AABB> buildWorldAABBs(const std::vector<std::unique_ptr<Object3D>>& objects) {
    std::vector<AABB> worldAABBs;
    worldAABBs.reserve(objects.size());
    for (size_t i = 0; i < objects.size(); ++i) {
        auto box = makeAABB(*objects[i]);
        box.id = static_cast<int>(i);
        worldAABBs.push_back(box);
    }
    return worldAABBs;
}

// ===================== Rendering helpers =====================
// Render all cubes (objects + drone) with object shader
static void renderSceneCubes(GLuint shader, GLuint cubeVAO,
                             const std::vector<std::unique_ptr<Object3D>>& objects,
                             const Drone& drone) {
    for (const auto& object : objects) object->draw(shader, cubeVAO);

    // Drone
    drawDroneBox(drone.p, drone.bodyScale, shader, cubeVAO);
}

static void renderGridLines(GLuint shader, GLuint gridVAO, int gridVertexCount,
                            const glm::mat4& proj, const glm::mat4& view) {

    // WICHTIG: Tiefentest deaktivieren, damit die Linien ÜBER der Bodenfläche gezeichnet werden,
    // ohne Z-Fighting, oder verwenden Sie einen Polygon-Offset.
    // glDisable(GL_DEPTH_TEST); 
    glEnable(GL_POLYGON_OFFSET_LINE);
    // glPolygonOffset(-2.0f, -2.0f);
    
    glUseProgram(shader);
    glUniformMatrix4fv(glGetUniformLocation(shader, "uProjection"), 1, GL_FALSE, glm::value_ptr(proj));
    glUniformMatrix4fv(glGetUniformLocation(shader, "uView"), 1, GL_FALSE, glm::value_ptr(view));
    
    // Model-Matrix muss auf Identity gesetzt werden, da die Linien im Weltraum sind.
    glm::mat4 model = glm::mat4(1.0f);
    glUniformMatrix4fv(glGetUniformLocation(shader, "uModel"), 1, GL_FALSE, glm::value_ptr(model));

    // Setzen Sie eine Gitterfarbe (heller als der Boden)
    glm::vec3 lineColor = glm::vec3(0.5f, 0.5f, 0.5f); 
    glUniform3fv(glGetUniformLocation(shader, "uColor"), 1, glm::value_ptr(lineColor));

    glBindVertexArray(gridVAO);
    glDrawArrays(GL_LINES, 0, gridVertexCount); // Zeichnet die Linien!
    glBindVertexArray(0);

    glDisable(GL_POLYGON_OFFSET_LINE);
    // glEnable(GL_DEPTH_TEST); // Tiefentest wieder aktivieren
}

// Render full grid using grid shader + VAO
static void renderGrid(GLuint shader, GLuint gridVAO, int gridVertexCount,
                       const glm::mat4& proj, const glm::mat4& view,
                       const glm::mat4& lightSpace, const glm::vec3& lightPos,
                       const ShadowMap& sm) {
    glUseProgram(shader);
    glUniformMatrix4fv(glGetUniformLocation(shader, "uProjection"), 1, GL_FALSE, glm::value_ptr(proj));
    glUniformMatrix4fv(glGetUniformLocation(shader, "uView"), 1, GL_FALSE, glm::value_ptr(view));

    glUniformMatrix4fv(glGetUniformLocation(shader, "uLightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpace));
    glUniform3fv(glGetUniformLocation(shader, "uLightPos"), 1, glm::value_ptr(lightPos));

    // Light Space Matrix (needed to calculate fragment position in light space)
    glUniformMatrix4fv(glGetUniformLocation(shader, "uLightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpace));

    sm.bindForRead(shader, 0); // Bind shadow map to texture unit 0

    glBindVertexArray(gridVAO);
    glDrawArrays(GL_LINES, 0, gridVertexCount);
    glBindVertexArray(0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);

    glUseProgram(0);
}

static void renderGround(GLuint shader, GLuint groundVAO,
                       const glm::mat4& proj, const glm::mat4& view,
                       const glm::mat4& lightSpace, const glm::vec3& lightPos,
                       const ShadowMap& sm // Die ShadowMap-Struktur
                       ) {
    glUseProgram(shader);

    // Camera Matrices
    glUniformMatrix4fv(glGetUniformLocation(shader, "uProjection"), 1, GL_FALSE, glm::value_ptr(proj));
    glUniformMatrix4fv(glGetUniformLocation(shader, "uView"), 1, GL_FALSE, glm::value_ptr(view));

    // Shadow Mapping Uniforms
    glUniformMatrix4fv(glGetUniformLocation(shader, "uLightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpace));
    glUniform3fv(glGetUniformLocation(shader, "uLightPos"), 1, glm::value_ptr(lightPos));

    // Bodenfarbe setzen
    glm::vec3 groundColor = glm::vec3(1.0f, 1.0f, 1.0f);
    glUniform3fv(glGetUniformLocation(shader, "uGroundColor"), 1, glm::value_ptr(groundColor));

    // Shadow Map Binden (Textur-Einheit 0)
    sm.bindForRead(shader, 0);

    glBindVertexArray(groundVAO);
    // WICHTIG: Draw call für Dreiecke
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    // Aufräumen
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

// One-pass depth render for shadow map
static void shadowPass(const ShadowMap& sm, GLuint shadowShader,
                       const glm::mat4& lightSpace,
                       GLuint cubeVAO,
                       const std::vector<std::unique_ptr<Object3D>>& objects,
                       const Drone& drone) {
    sm.bindForWrite();
    glUseProgram(shadowShader);
    glUniformMatrix4fv(glGetUniformLocation(shadowShader, "uLightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpace));

    // Draw objects using the same VAO; shader uses uModel only
    // Reuse the same routine which sets uModel for each object
    // but bind shadow shader instead of object shader.
    for (const auto& obj : objects) obj->draw(shadowShader, cubeVAO);
    drawDroneBox(drone.p, drone.bodyScale, shadowShader, cubeVAO);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// Main pass (color) with shadow sampling
static void mainPass(GLuint objectShader, GLuint cubeVAO,
                     const glm::mat4& proj, const glm::mat4& view,
                     const glm::mat4& lightSpace, const glm::vec3& lightPos,
                     const ShadowMap& sm,
                     const std::vector<std::unique_ptr<Object3D>>& objects,
                     const Drone& drone) {
    // glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    glUseProgram(objectShader);
    glUniformMatrix4fv(glGetUniformLocation(objectShader, "uProjection"), 1, GL_FALSE, glm::value_ptr(proj));
    glUniformMatrix4fv(glGetUniformLocation(objectShader, "uView"), 1, GL_FALSE, glm::value_ptr(view));
    glUniform3fv(glGetUniformLocation(objectShader, "uLightPos"), 1, glm::value_ptr(lightPos));
    glUniformMatrix4fv(glGetUniformLocation(objectShader, "uLightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpace));

    sm.bindForRead(objectShader, 0);

    renderSceneCubes(objectShader, cubeVAO, objects, drone);

    glUseProgram(0);
    // glDisable(GL_DEPTH_TEST);
}

// ===================== Application (window, loop) =====================
int main() {
    std::cout << "=== UAM Simulator — modularized ===\n";

    // ---- Create window and GL context
    sf::ContextSettings settings;
    settings.depthBits = 24;
    settings.stencilBits = 8;
    settings.antiAliasingLevel = 4;
    settings.majorVersion = 4;
    settings.minorVersion = 1;
    settings.attributeFlags = sf::ContextSettings::Core;

    sf::RenderWindow window(
        sf::VideoMode({1200, 900}),
        "UAM Simulator",
        sf::Style::Default,
        sf::State::Windowed,
        settings
    );
    if (!window.setActive(true)) std::cerr << "Warning: Could not activate OpenGL context!\n";

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) { std::cerr << "GLEW init failed\n"; return -1; }

    window.setVerticalSyncEnabled(true);
    std::cout << "OpenGL: " << glGetString(GL_VERSION) << " | Renderer: " << glGetString(GL_RENDERER) << "\n";

    // ---- Compile shaders
    GLuint gridShader   = glutil::createShaderProgram("grid.vert", "grid.frag");
    GLuint objectShader = glutil::createShaderProgram("object.vert", "object.frag");
    GLuint shadowShader = glutil::createShaderProgram("shadow.vert", "shadow.frag");
    GLuint pointLineShaderProgram = glutil::createShaderProgram("point_line.vert", "point_line.frag");
    if (!gridShader || !objectShader || !shadowShader || !pointLineShaderProgram) return -1;

    // ---- Create geometry
    GLuint gridVAO = 0, gridVBO = 0; int gridVertexCount = 0;
    createGridVAO(gridVAO, gridVBO, gridVertexCount);

    GLuint cubeVAO = 0, cubeVBO = 0;
    createCubeVAO(cubeVAO, cubeVBO);

    GLuint sensorVAO = 0, sensorVBO = 0; 
    createSensorVAO(sensorVAO, sensorVBO);

    GLuint groundVAO = 0, groundVBO = 0;
    createGroundVAO(groundVAO, groundVBO);

    // ---- Shadow map
    ShadowMap shadow; shadow.init();

    // ---- Static GL state
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // ---- Scene objects
    auto objects = loadObjectsOrDefault("config.yaml");
    auto worldAABBs = buildWorldAABBs(objects);

    // ---- Lighting
    glm::vec3 lightPos(5.0f, 8.0f, 3.0f);
    bool enableShadows = true;

    // ---- Camera & modes
    Camera cam;
    float yawSpeed = 60.f;
    bool enableMouseLook = false;
    bool followDrone = true;
    sf::Vector2i windowCenter(window.getSize().x / 2, window.getSize().y / 2);

    // ---- Drone & sensors
    Drone drone; drone.p.yaw = -90.f;

    bool enableLidar = false;
    bool enableRadar = false;
    bool enableLightSource = false;
    RadarParams radarCfg;
    glm::vec3 prevDronePos = drone.p.pos, droneVel{0,0,0};
    bool prevPosValid = false;

    // ---- Lidar config
    const int   beamsH   = 360;
    const int   beamsV   = 16;
    const float fovH_deg = 360.f;
    const float fovV_deg = 45.f;
    const float lidarMax = 50.f;

    // ---- Timing
    sf::Clock deltaClock, fpsCounter;
    int frameCount = 0;
    float dtSmooth = 1.f/60.f;

    // ---- Events handlers (use your window.handleEvents helper)
    const auto onClose = [&](const sf::Event::Closed&) { window.close(); };
    const auto onKeyPressed = [&](const sf::Event::KeyPressed& keyPressed) {
        using sc = sf::Keyboard::Scancode;
        if (keyPressed.scancode == sc::M) {
            enableMouseLook = !enableMouseLook;
            window.setMouseCursorVisible(!enableMouseLook);
            if (enableMouseLook) sf::Mouse::setPosition(windowCenter, window);
            std::cout << "Mouse look: " << (enableMouseLook ? "ON" : "OFF") << "\n";
        } else if (keyPressed.scancode == sc::B) {
            enableLightSource = !enableLightSource;
            std::cout << "Light source cube: " << (enableLightSource ? "ON" : "OFF") << "\n";
        } else if (keyPressed.scancode == sc::H) {
            enableShadows = !enableShadows;
            std::cout << "Shadows: " << (enableShadows ? "ON" : "OFF") << "\n";
        } else if (keyPressed.scancode == sc::F) {
            followDrone = !followDrone;
            std::cout << "Follow drone: " << (followDrone ? "ON" : "OFF") << "\n";
        } else if (keyPressed.scancode == sc::Escape) {
            window.close();
        } else if (keyPressed.scancode == sc::L) {
            enableLidar = !enableLidar;
            std::cout << "Lidar: " << (enableLidar ? "ON" : "OFF") << "\n";
        } else if (keyPressed.scancode == sc::R) {
            enableRadar = !enableRadar;
            std::cout << "Radar: " << (enableRadar ? "ON" : "OFF") << "\n";
        }

        // Light control (arrow keys + PageUp/Down)
        float lightSpeed = 0.5f;
        if (keyPressed.scancode == sc::Up)       lightPos.z -= lightSpeed;
        if (keyPressed.scancode == sc::Down)     lightPos.z += lightSpeed;
        if (keyPressed.scancode == sc::Left)     lightPos.x -= lightSpeed;
        if (keyPressed.scancode == sc::Right)    lightPos.x += lightSpeed;
        if (keyPressed.scancode == sc::PageUp)   lightPos.y += lightSpeed;
        if (keyPressed.scancode == sc::PageDown) lightPos.y -= lightSpeed;
    };
    const auto onMouseMoved = [&](const sf::Event::MouseMoved& e) {
        if (enableMouseLook && !followDrone) {
            float xoffset = static_cast<float>(e.position.x - windowCenter.x);
            float yoffset = static_cast<float>(windowCenter.y - e.position.y);
            sf::Mouse::setPosition(windowCenter, window);

            float sensitivity = 0.1f;
            cam.yaw   += xoffset * sensitivity;
            cam.pitch += yoffset * sensitivity;
            cam.pitch = glm::clamp(cam.pitch, -89.f, 89.f);

            cam.front = cam.calcFront(cam.yaw, cam.pitch);
            cam.right = glm::normalize(glm::cross(cam.front, cam.up));
        }
    };

    std::cout << "\n=== Controls ===\n"
              << "WASD: move, Q/E: yaw (drone), C/V: up/down\n"
              << "F: follow cam, M: mouse look (free cam), L: LiDAR, R: RADAR, H: shadows, ESC: quit\n"
              << "Arrows/PgUp/PgDn: move light\n";

    // ---- Main loop
    while (window.isOpen()) {
        float dt = deltaClock.restart().asSeconds();
        dt = std::min(dt, 0.1f);
        dtSmooth = 0.9f * dtSmooth + 0.1f * dt;

        window.handleEvents(
            onClose,
            [](const sf::Event::Resized&) {},
            onKeyPressed,
            [](const sf::Event::KeyReleased&) {},
            [](const sf::Event::MouseButtonPressed&) {},
            [](const sf::Event::MouseButtonReleased&) {},
            onMouseMoved,
            [](const sf::Event::MouseWheelScrolled&) {}
        );

        // ---- Update camera/drone
        if (!followDrone) {
            // Free camera movement and yaw (Q/E) like before
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::E)) {
                cam.yaw += yawSpeed * dtSmooth;
                cam.front = cam.calcFront(cam.yaw, cam.pitch);
                cam.right = glm::normalize(glm::cross(cam.front, cam.up));
            }
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Q)) {
                cam.yaw -= yawSpeed * dtSmooth;
                cam.front = cam.calcFront(cam.yaw, cam.pitch);
                cam.right = glm::normalize(glm::cross(cam.front, cam.up));
            }
            updateFreeCamera(cam, dtSmooth, 5.0f);
        } else {
            updateDrone(drone, dtSmooth);

            if (!prevPosValid) { prevPosValid = true; prevDronePos = drone.p.pos; }
            droneVel = (drone.p.pos - prevDronePos) / std::max(dtSmooth, 1e-4f);
            prevDronePos = drone.p.pos;

            updateFollowCamera(drone, cam);
        }

        // ---- Matrices
        glClearColor(0.8f, 0.9f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float aspect = static_cast<float>(window.getSize().x) / static_cast<float>(window.getSize().y);
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspect, 0.5f, 80.0f);
        glm::mat4 view = glm::lookAt(cam.pos, cam.pos + cam.front, cam.up);

        // ---- Light space (orthographic, directional-like)
        float nearPlane = 1.0f, farPlane = 70.0f;
        glm::mat4 lightProj = glm::ortho(-20.0f, 20.0f, -20.0f, 20.0f, nearPlane, farPlane);
        glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0.0f, 0.0f, 0.0f), cam.up);
        glm::mat4 lightSpace = lightProj * lightView;

        // ---- Pass 1: Shadow depth
        if (enableShadows) {
            shadowPass(shadow, shadowShader, lightSpace, cubeVAO, objects, drone);
        }

        // ---- Pass 2: Grid + scene
        glViewport(0, 0, window.getSize().x, window.getSize().y);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Grid (no lighting)
        
        renderGround(gridShader, groundVAO, projection, view, lightSpace, lightPos, shadow);
        
        // Opaque scene with shadow map
        mainPass(objectShader, cubeVAO, projection, view, lightSpace, lightPos, shadow, objects, drone);
        
        renderGridLines(pointLineShaderProgram, gridVAO, gridVertexCount, projection, view);
        // renderGrid(gridShader, gridVAO, gridVertexCount, projection, view, lightSpace, lightPos, shadow);

        if (enableLightSource) {
            drawLightSource(lightPos, pointLineShaderProgram, cubeVAO, view, projection);
        }

        // ---- Sensors (debug render)
        auto hits = simulateLidar3D(drone.p, worldAABBs, beamsH, beamsV, fovH_deg, fovV_deg, lidarMax);
        if (enableLidar) {
            // LiDAR Punkte (Point Cloud)
            std::vector<glm::vec3> lidarPoints;
            std::vector<glm::vec3> lidarBeams;
            for (const auto& hit : hits) {
                if (hit.ok) {
                    lidarPoints.push_back(hit.point);
                    // Lidar Beams (Linien vom Zentrum zu den Punkten)
                    lidarBeams.push_back(drone.p.pos);
                    lidarBeams.push_back(hit.point);
                }
            }
            
            // Punkte zeichnen (rot)
            drawPoints(lidarPoints, glm::vec3(1.0f, 0.0f, 0.0f), 3.0f, pointLineShaderProgram, sensorVAO, sensorVBO, view, projection);
            // Linien zeichnen 
            drawLines(lidarBeams, glm::vec3(1.0f, 0.0f, 0.0f), 1.0f, pointLineShaderProgram, sensorVAO, sensorVBO, view, projection);
        }

        auto radarDets = simulateRadar3D(drone.p, droneVel, worldAABBs, radarCfg);
        if (enableRadar) {
            std::vector<glm::vec3> radarPoints;
            std::vector<glm::vec3> radarBeams;
            
            // Fülle die Daten
            for (const auto& det : radarDets) {
                if (det.ok) {
                    radarPoints.push_back(det.point);
                    radarBeams.push_back(drone.p.pos);
                    radarBeams.push_back(det.point);
                }
            }
            
            // Punkte zeichnen (Cyan/Grün - VR-Färbung wurde der Einfachheit halber im Helper auf festes Grün gesetzt)
            // Hinweis: Für eine dynamische Färbung nach vr müsste ein Shader mit einem zusätzlichen VBO für die Farbe verwendet werden.
            drawPoints(radarPoints, glm::vec3(0.0f, 1.0f, 0.0f), 2.0f, pointLineShaderProgram, sensorVAO, sensorVBO, view, projection);
            
            // Linien zeichnen (Cyan)
            drawLines(radarBeams, glm::vec3(0.0f, 0.9f, 0.9f), 1.0f, pointLineShaderProgram, sensorVAO, sensorVBO, view, projection);
        }

        window.display();

        // ---- FPS
        frameCount++;
        if (fpsCounter.getElapsedTime().asSeconds() >= 1.0f) {
            std::cout << "FPS: " << frameCount << " | Light: ("
                      << lightPos.x << ", " << lightPos.y << ", " << lightPos.z << ")\n";
            frameCount = 0;
            fpsCounter.restart();
        }
    }

    // ---- Cleanup
    glDeleteVertexArrays(1, &gridVAO);
    glDeleteBuffers(1, &gridVBO);
    glDeleteProgram(gridShader);
    
    glDeleteVertexArrays(1, &groundVAO);
    glDeleteBuffers(1, &groundVBO);

    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteProgram(objectShader);
    glDeleteProgram(shadowShader);

    glDeleteVertexArrays(1, &sensorVAO);
    glDeleteBuffers(1, &sensorVBO);
    glDeleteProgram(pointLineShaderProgram);

    std::cout << "Exiting successfully!\n";
    return 0;
}

// ===================== Implementations left as-is =====================
static std::vector<glm::vec3> convexHullXZ(std::vector<glm::vec3> pts) {
    if (pts.size() <= 3) return pts;
    std::sort(pts.begin(), pts.end(), [](const glm::vec3& a, const glm::vec3& b){
        if (a.x != b.x) return a.x < b.x;
        return a.z < b.z;
    });
    auto cross2D = [](const glm::vec3& O, const glm::vec3& A, const glm::vec3& B){
        return (A.x - O.x)*(B.z - O.z) - (A.z - O.z)*(B.x - O.x);
    };
    std::vector<glm::vec3> H; H.reserve(pts.size()*2);
    for (const auto& p : pts){
        while (H.size() >= 2 && cross2D(H[H.size()-2], H.back(), p) <= 0.f) H.pop_back();
        H.push_back(p);
    }
    size_t t = H.size()+1;
    for (int i=(int)pts.size()-2; i>=0; --i){
        const auto& p = pts[i];
        while (H.size() >= t && cross2D(H[H.size()-2], H.back(), p) <= 0.f) H.pop_back();
        H.push_back(p);
    }
    H.pop_back();
    return H;
}