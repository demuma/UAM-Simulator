#version 410 core

// 0 = Position (vec3)
// 1 = Farbe (vec3)
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;

// Output zur Fragment Shader
out vec3 vColor;

// Matrizen, die wir von C++ (glm) bekommen
uniform mat4 uProjection;
uniform mat4 uView;
// (Wir brauchen keine Model-Matrix für das Gitter, da es bei (0,0,0) ist)

void main()
{
    // Berechne die finale Position auf dem Bildschirm
    gl_Position = uProjection * uView * vec4(aPos, 1.0);
    // Gib die Farbe einfach weiter
    vColor = aColor;
}