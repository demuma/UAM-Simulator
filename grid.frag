#version 410 core

// Input vom Vertex Shader
in vec3 vColor;

// Output (die endgültige Pixelfarbe)
out vec4 FragColor;

void main()
{
    FragColor = vec4(vColor, 1.0);
}