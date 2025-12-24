#ifndef SHADERS_H
#define SHADERS_H

#include <GL/glew.h>
#include <iostream>

const char* vs_source = R"(#version 330 core
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texCoords;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec3 tangent;
layout(location = 4) in float type;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float time;
uniform bool windEffect;
uniform float windStrength;
uniform float windFrequency;
uniform float treeHeight;
uniform float windOffset;

out vec2 TexCoords;
out vec3 FragPos;
out vec3 Normal;
out vec3 Tangent;
out float Type;

void main() {
    vec3 pos = position;
    
    if (windEffect && type > 1.5) { 
        float mainWind = sin(time * windFrequency * 0.7 + windOffset * 0.01) * windStrength;
        
        float secondaryWind = sin(time * windFrequency * 2.3 + position.x * 0.1 + windOffset * 0.02) * windStrength * 0.3;
        
        float heightFactor = pos.y / treeHeight;
        heightFactor = heightFactor * heightFactor; 
        
        if (pos.y > 5.0) {
            float windX = (mainWind + secondaryWind) * heightFactor * 0.5;
            float windZ = cos(time * windFrequency * 0.9 + windOffset * 0.015) * windStrength * heightFactor * 0.3;
            
            float branchFactor = length(pos.xz) / 2.5; // 2.5 - радиус кроны
            branchFactor = smoothstep(0.0, 1.0, branchFactor);
            
            pos.x += windX * branchFactor;
            pos.z += windZ * branchFactor;
            
            pos.y += sin(time * windFrequency * 1.2 + windOffset * 0.01) * windStrength * 0.1 * heightFactor * branchFactor;
        }
        
        if (pos.y > 3.0 && pos.y < 7.0) {
            float trunkWind = sin(time * windFrequency * 0.3 + windOffset * 0.005) * windStrength * 0.1;
            float trunkHeightFactor = (pos.y - 3.0) / 4.0; // От 3 до 7
            pos.x += trunkWind * trunkHeightFactor;
        }
    }
    
    FragPos = vec3(model * vec4(pos, 1.0));
    TexCoords = texCoords;
    Normal = mat3(transpose(inverse(model))) * normal;
    Tangent = tangent;
    Type = type;
    
    gl_Position = projection * view * model * vec4(pos, 1.0);
})";

const char* fs_source = R"(#version 330 core
out vec4 FragColor;

in vec2 TexCoords;
in vec3 FragPos;
in vec3 Normal;
in vec3 Tangent;
in float Type;

uniform sampler2D texture0;
uniform sampler2D texture1;
uniform vec3 lightDir;
uniform vec3 baseColor;
uniform bool useTexture;
uniform bool useNormalMap;
uniform float time;

vec3 calculateNormal() {
    vec3 normalMap = texture(texture1, TexCoords).rgb;
    
    normalMap = normalize(normalMap * 2.0 - 1.0);
    
    vec3 N = normalize(Normal);
    vec3 T = normalize(Tangent);
    
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);
    
    mat3 TBN = mat3(T, B, N);
    
    return normalize(TBN * normalMap);
}

void main() {
    vec3 color = baseColor;
    
    if (useTexture) {
        color = texture(texture0, TexCoords).rgb;
    }
    
    if (Type > 0.5) {
        color = baseColor;
    }
    
    vec3 norm;
    if (useNormalMap) {
        norm = calculateNormal();
    } else {
        norm = normalize(Normal);
    }
    
    vec3 lightColor = vec3(1.0, 1.0, 0.9);
    vec3 ambient = vec3(0.3) * lightColor;
    
    float diff = max(dot(norm, -lightDir), 0.0);
    vec3 diffuse = diff * lightColor;
    
    vec3 viewDir = normalize(-FragPos);
    vec3 reflectDir = reflect(lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = spec * vec3(0.3, 0.3, 0.3);
    
    vec3 result = (ambient + diffuse + specular) * color;
    
    FragColor = vec4(result, 1.0);
})";

inline unsigned int CreateShaderProgram() {
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vs_source, NULL);
    glCompileShader(vertexShader);

    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cout << "Vertex shader compilation failed:\n" << infoLog << std::endl;
    }

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fs_source, NULL);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cout << "Fragment shader compilation failed:\n" << infoLog << std::endl;
    }

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cout << "Shader program linking failed:\n" << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

#endif