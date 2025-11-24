#version 410 core

// Inputs vom Vertex Shader
in vec3 vFragPos;
in vec3 vNormal;

// Output-Farbe
out vec4 FragColor;

// Uniforms (Daten vom C++ Code)
uniform vec3 uObjectColor; // Die Grundfarbe des Objekts
uniform vec3 uLightPos;    // Die Position der Lichtquelle

void main()
{
    float ambientStrength = 0.3; // Umgebungslicht
    vec3 ambient = ambientStrength * uObjectColor;
    
    // Diffuse Beleuchtung
    vec3 norm = normalize(vNormal);
    vec3 lightDir = normalize(uLightPos - vFragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * uObjectColor; // Lichtfarbe ist hier mal weiß
    
    vec3 result = ambient + diffuse;
    FragColor = vec4(result, 1.0);
}