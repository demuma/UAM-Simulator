#version 410 core

// 0 = Position (vec3)
layout (location = 0) in vec3 aPos;
// 1 = Normale (vec3) - NEU
layout (location = 1) in vec3 aNormal; 

// Output an Fragment Shader
out vec3 vNormal;         
out vec4 vLightSpacePos; // Position des Vertex im Licht-Space

// Matrizen
uniform mat4 uProjection;
uniform mat4 uView;
uniform mat4 uLightSpaceMatrix; // Neu

void main()
{
    gl_Position = uProjection * uView * vec4(aPos, 1.0);
    
    // Grid lies on XZ plane; use fixed normal
    vNormal = vec3(0.0, 1.0, 0.0);
    
    // Position des Vertex im Light-Space
    // Da das Gitter keine Model-Matrix hat (Identity), verwenden wir nur aPos.
    vLightSpacePos = uLightSpaceMatrix * vec4(aPos, 1.0);
}
