#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <ctime>
#include <random>

#include "camera.h"
#include "shaders.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

const int NUM_HOUSES = 10;
const int NUM_TREES = 15;
const int NUM_ROCKS = 10;
const int NUM_PACKAGES_MAX = 20;

struct House {
    glm::vec3 position;
    bool hasPackage;
    int houseType; 
};

struct Package {
    glm::vec3 position;
    glm::vec3 velocity;
    bool active;
    float rotation;
    float rotationSpeed;
};

struct TreeObject {
    glm::vec3 position;
    float windOffset;
    float treeHeight;
};

struct Vertex {
    glm::vec3 position;
    glm::vec2 texCoords;
    glm::vec3 normal;
    glm::vec3 tangent;
    float type;
};

struct GameObject {
    unsigned int vao;
    unsigned int texture;
    unsigned int normalMap;
    int vertexCount;
    glm::vec3 baseColor;
    std::string name;
};

std::vector<House> houses;
std::vector<TreeObject> treePositions;
std::vector<glm::vec3> rockPositions;
std::vector<Package> packages;

GameObject airship, field, tree, rock, house1, house2, house3, packageObj;
Camera camera;
glm::vec3 airshipPosition(0, 100, 0);
float airshipSpeed = 50.0f;
float airshipRotation = 0.0f;
bool aimMode = false;
bool cPressed = false;
float windTime = 0.0f;
int deliveredPackages = 0;
int totalHouses = 0;
bool gameStarted = false;

void generate_package(std::vector<Vertex>& vertices);
void computeTangents(std::vector<Vertex>& vertices);

unsigned int load_texture(const char* path) {
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path, &width, &height, &nrComponents, 0);

    if (data) {
        GLenum format = GL_RGB;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        std::cout << "Texture loaded successfully: " << path
            << " (" << width << "x" << height << ", channels: " << nrComponents << ")" << std::endl;

        stbi_image_free(data);
    }
    else {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        if (data) stbi_image_free(data);

        unsigned char fallbackData[] = {
            200, 200, 200,   100, 100, 100,
            100, 100, 100,   200, 200, 200
        };

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, fallbackData);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    return textureID;
}

void computeTangents(std::vector<Vertex>& vertices) {
    for (size_t i = 0; i < vertices.size(); i += 3) {
        if (i + 2 >= vertices.size()) break;

        Vertex& v0 = vertices[i];
        Vertex& v1 = vertices[i + 1];
        Vertex& v2 = vertices[i + 2];

        glm::vec3 edge1 = v1.position - v0.position;
        glm::vec3 edge2 = v2.position - v0.position;
        glm::vec2 deltaUV1 = v1.texCoords - v0.texCoords;
        glm::vec2 deltaUV2 = v2.texCoords - v0.texCoords;

        float f = 1.0f;
        float denom = deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y;
        if (fabs(denom) > 0.0001f) {
            f = 1.0f / denom;
        }

        glm::vec3 tangent;
        tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
        tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
        tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);

        if (glm::length(tangent) > 0.0001f) {
            tangent = glm::normalize(tangent);
        }
        else {
            tangent = glm::vec3(1.0f, 0.0f, 0.0f);
        }

        v0.tangent = tangent;
        v1.tangent = tangent;
        v2.tangent = tangent;
    }
}

void generate_package(std::vector<Vertex>& vertices) {
    float size = 0.5f;

    glm::vec3 verticesList[8] = {
        {-size, -size, -size},
        {size, -size, -size},
        {size, -size, size},
        {-size, -size, size},
        {-size, size, -size},
        {size, size, -size},
        {size, size, size},
        {-size, size, size}
    };

    int faces[6][4] = {
        {0,1,2,3}, // низ
        {4,5,6,7}, // верх
        {0,1,5,4}, // перед
        {2,3,7,6}, // зад
        {0,3,7,4}, // лево
        {1,2,6,5}  // право
    };

    glm::vec3 normals[6] = {
        {0,-1,0}, {0,1,0}, {0,0,-1}, {0,0,1}, {-1,0,0}, {1,0,0}
    };

    for (int face = 0; face < 6; face++) {
        glm::vec3 normal = normals[face];

        vertices.push_back({ verticesList[faces[face][0]], {0,0}, normal, {1,0,0}, 1.0f });
        vertices.push_back({ verticesList[faces[face][1]], {1,0}, normal, {1,0,0}, 1.0f });
        vertices.push_back({ verticesList[faces[face][2]], {1,1}, normal, {1,0,0}, 1.0f });

        vertices.push_back({ verticesList[faces[face][0]], {0,0}, normal, {1,0,0}, 1.0f });
        vertices.push_back({ verticesList[faces[face][2]], {1,1}, normal, {1,0,0}, 1.0f });
        vertices.push_back({ verticesList[faces[face][3]], {0,1}, normal, {1,0,0}, 1.0f });
    }

    computeTangents(vertices);
}

void load_obj(const std::string& path, std::vector<Vertex>& vertices) {
    generate_package(vertices);
}

void generate_terrain(std::vector<Vertex>& vertices) {
    int width = 10;
    int height = 10;
    float scale = 500.0f;

    for (int z = 0; z < height - 1; z++) {
        for (int x = 0; x < width - 1; x++) {
            float x0 = (x / (float)(width - 1) - 0.5f) * scale;
            float x1 = ((x + 1) / (float)(width - 1) - 0.5f) * scale;
            float z0 = (z / (float)(height - 1) - 0.5f) * scale;
            float z1 = ((z + 1) / (float)(height - 1) - 0.5f) * scale;

            float y00 = 0.0f;
            float y10 = 0.0f;
            float y01 = 0.0f;
            float y11 = 0.0f;

            glm::vec3 p00(x0, y00, z0);
            glm::vec3 p10(x1, y10, z0);
            glm::vec3 p01(x0, y01, z1);
            glm::vec3 p11(x1, y11, z1);

            // Нормали (вверх)
            glm::vec3 normal(0, 1, 0);

            // Первый треугольник
            vertices.push_back({ p00, {0,0}, normal, {1,0,0}, 0.0f });
            vertices.push_back({ p10, {1,0}, normal, {1,0,0}, 0.0f });
            vertices.push_back({ p01, {0,1}, normal, {1,0,0}, 0.0f });

            // Второй треугольник
            vertices.push_back({ p10, {1,0}, normal, {1,0,0}, 0.0f });
            vertices.push_back({ p11, {1,1}, normal, {1,0,0}, 0.0f });
            vertices.push_back({ p01, {0,1}, normal, {1,0,0}, 0.0f });
        }
    }
}

void generate_tree(std::vector<Vertex>& vertices, float height = 12.0f) {
    float trunkRadius = 0.8f;
    float trunkHeight = height * 0.6f;
    float crownRadius = 2.5f;
    float crownHeight = height * 0.4f;

    // Ствол 
    int trunkSegments = 8;
    for (int i = 0; i < trunkSegments; i++) {
        float angle1 = 2.0f * M_PI * i / trunkSegments;
        float angle2 = 2.0f * M_PI * (i + 1) / trunkSegments;

        // Нижние точки
        glm::vec3 p1(cos(angle1) * trunkRadius, 0, sin(angle1) * trunkRadius);
        glm::vec3 p2(cos(angle2) * trunkRadius, 0, sin(angle2) * trunkRadius);

        // Верхние точки
        glm::vec3 p3 = p1 + glm::vec3(0, trunkHeight, 0);
        glm::vec3 p4 = p2 + glm::vec3(0, trunkHeight, 0);

        // Нормали для ствола
        glm::vec3 normal1 = glm::normalize(glm::vec3(p1.x, 0, p1.z));
        glm::vec3 normal2 = glm::normalize(glm::vec3(p2.x, 0, p2.z));

        // Треугольник 1
        vertices.push_back({ p1, {0,0}, normal1, {1,0,0}, 2.0f }); 
        vertices.push_back({ p2, {1,0}, normal2, {1,0,0}, 2.0f });
        vertices.push_back({ p3, {0,1}, normal1, {1,0,0}, 2.0f });

        // Треугольник 2
        vertices.push_back({ p2, {1,0}, normal2, {1,0,0}, 2.0f });
        vertices.push_back({ p4, {1,1}, normal2, {1,0,0}, 2.0f });
        vertices.push_back({ p3, {0,1}, normal1, {1,0,0}, 2.0f });
    }

    // Крона дерева 
    int crownSegments = 16;
    glm::vec3 crownTop(0, trunkHeight + crownHeight, 0);

    for (int i = 0; i < crownSegments; i++) {
        float angle1 = 2.0f * M_PI * i / crownSegments;
        float angle2 = 2.0f * M_PI * (i + 1) / crownSegments;

        glm::vec3 p1(cos(angle1) * crownRadius, trunkHeight, sin(angle1) * crownRadius);
        glm::vec3 p2(cos(angle2) * crownRadius, trunkHeight, sin(angle2) * crownRadius);

        glm::vec3 normal1 = glm::normalize(glm::vec3(p1.x, crownRadius * 0.5f, p1.z));
        glm::vec3 normal2 = glm::normalize(glm::vec3(p2.x, crownRadius * 0.5f, p2.z));

        vertices.push_back({ p1, {0,0}, normal1, {1,0,0}, 2.0f });
        vertices.push_back({ p2, {1,0}, normal2, {1,0,0}, 2.0f });
        vertices.push_back({ crownTop, {0.5,1}, glm::normalize(normal1 + normal2), {1,0,0}, 2.0f });
    }

    for (int i = 0; i < crownSegments; i++) {
        float angle1 = 2.0f * M_PI * i / crownSegments;
        float angle2 = 2.0f * M_PI * (i + 1) / crownSegments;

        glm::vec3 p1(cos(angle1) * crownRadius, trunkHeight, sin(angle1) * crownRadius);
        glm::vec3 p2(cos(angle2) * crownRadius, trunkHeight, sin(angle2) * crownRadius);
        glm::vec3 center(0, trunkHeight, 0);

        glm::vec3 normal(0, -1, 0); 

        vertices.push_back({ center, {0.5,0.5}, normal, {1,0,0}, 2.0f });
        vertices.push_back({ p1, {0,0}, normal, {1,0,0}, 2.0f });
        vertices.push_back({ p2, {1,0}, normal, {1,0,0}, 2.0f });
    }
}

void generate_rock(std::vector<Vertex>& vertices) {
    float size = 3.0f;

    glm::vec3 verticesList[8] = {
        {-size, -size, -size},
        {size, -size, -size},
        {size, -size, size},
        {-size, -size, size},
        {-size, size, -size},
        {size, size, -size},
        {size, size, size},
        {-size, size, size}
    };

    int faces[6][4] = {
        {0,1,2,3}, {4,5,6,7}, {0,1,5,4},
        {2,3,7,6}, {0,3,7,4}, {1,2,6,5}
    };

    glm::vec3 normals[6] = {
        {0,-1,0}, {0,1,0}, {0,0,-1}, {0,0,1}, {-1,0,0}, {1,0,0}
    };

    for (int face = 0; face < 6; face++) {
        glm::vec3 normal = normals[face];

        vertices.push_back({ verticesList[faces[face][0]], {0,0}, normal, {1,0,0}, 0.0f });
        vertices.push_back({ verticesList[faces[face][1]], {1,0}, normal, {1,0,0}, 0.0f });
        vertices.push_back({ verticesList[faces[face][2]], {1,1}, normal, {1,0,0}, 0.0f });

        vertices.push_back({ verticesList[faces[face][0]], {0,0}, normal, {1,0,0}, 0.0f });
        vertices.push_back({ verticesList[faces[face][2]], {1,1}, normal, {1,0,0}, 0.0f });
        vertices.push_back({ verticesList[faces[face][3]], {0,1}, normal, {1,0,0}, 0.0f });
    }
}

void generate_house(std::vector<Vertex>& vertices, int type) {
    // Куб с крышей
    float size = 5.0f;
    float height = 8.0f;

    // Основание дома
    glm::vec3 base[8] = {
        {-size, 0, -size}, {size, 0, -size}, {size, 0, size}, {-size, 0, size},
        {-size, height, -size}, {size, height, -size}, {size, height, size}, {-size, height, size}
    };

    int faces[6][4] = {
        {0,1,2,3}, {4,5,6,7}, {0,1,5,4},
        {2,3,7,6}, {0,3,7,4}, {1,2,6,5}
    };

    glm::vec3 normals[6] = {
        {0,-1,0}, {0,1,0}, {0,0,-1}, {0,0,1}, {-1,0,0}, {1,0,0}
    };

    for (int face = 0; face < 6; face++) {
        glm::vec3 normal = normals[face];

        vertices.push_back({ base[faces[face][0]], {0,0}, normal, {1,0,0}, 0.0f });
        vertices.push_back({ base[faces[face][1]], {1,0}, normal, {1,0,0}, 0.0f });
        vertices.push_back({ base[faces[face][2]], {1,1}, normal, {1,0,0}, 0.0f });

        vertices.push_back({ base[faces[face][0]], {0,0}, normal, {1,0,0}, 0.0f });
        vertices.push_back({ base[faces[face][2]], {1,1}, normal, {1,0,0}, 0.0f });
        vertices.push_back({ base[faces[face][3]], {0,1}, normal, {1,0,0}, 0.0f });
    }
}

void generate_airship(std::vector<Vertex>& vertices) {
    float radiusX = 10.0f;
    float radiusY = 5.0f;
    float radiusZ = 20.0f;

    int stacks = 8;
    int sectors = 16;

    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < sectors; ++j) {
            float phi1 = M_PI * i / stacks;
            float phi2 = M_PI * (i + 1) / stacks;
            float theta1 = 2 * M_PI * j / sectors;
            float theta2 = 2 * M_PI * (j + 1) / sectors;

            glm::vec3 p1(
                radiusX * sin(phi1) * cos(theta1),
                radiusY * cos(phi1),
                radiusZ * sin(phi1) * sin(theta1)
            );

            glm::vec3 p2(
                radiusX * sin(phi1) * cos(theta2),
                radiusY * cos(phi1),
                radiusZ * sin(phi1) * sin(theta2)
            );

            glm::vec3 p3(
                radiusX * sin(phi2) * cos(theta1),
                radiusY * cos(phi2),
                radiusZ * sin(phi2) * sin(theta1)
            );

            glm::vec3 p4(
                radiusX * sin(phi2) * cos(theta2),
                radiusY * cos(phi2),
                radiusZ * sin(phi2) * sin(theta2)
            );

            glm::vec3 normal1 = glm::normalize(p1);
            glm::vec3 normal2 = glm::normalize(p2);
            glm::vec3 normal3 = glm::normalize(p3);
            glm::vec3 normal4 = glm::normalize(p4);

            glm::vec3 tangent = glm::normalize(p2 - p1);

            if (i == 0) {
                vertices.push_back({ p1, {0,0}, normal1, tangent, 0.0f });
                vertices.push_back({ p2, {1,0}, normal2, tangent, 0.0f });
                vertices.push_back({ p3, {0,1}, normal3, tangent, 0.0f });
            }
            else if (i == stacks - 1) {
                vertices.push_back({ p3, {0,1}, normal3, tangent, 0.0f });
                vertices.push_back({ p2, {1,0}, normal2, tangent, 0.0f });
                vertices.push_back({ p4, {1,1}, normal4, tangent, 0.0f });
            }
            else {
                vertices.push_back({ p1, {0,0}, normal1, tangent, 0.0f });
                vertices.push_back({ p2, {1,0}, normal2, tangent, 0.0f });
                vertices.push_back({ p4, {1,1}, normal4, tangent, 0.0f });

                vertices.push_back({ p1, {0,0}, normal1, tangent, 0.0f });
                vertices.push_back({ p4, {1,1}, normal4, tangent, 0.0f });
                vertices.push_back({ p3, {0,1}, normal3, tangent, 0.0f });
            }
        }
    }
}

GameObject create_object(const std::string& type, const std::string& texturePath = "",
    const std::string& normalPath = "", const glm::vec3& color = glm::vec3(1.0f)) {

    std::vector<Vertex> vertices;

    if (type == "FIELD") {
        generate_terrain(vertices);
    }
    else if (type == "TREE") {
        generate_tree(vertices, 12.0f);
    }
    else if (type == "ROCK") {
        generate_rock(vertices);
    }
    else if (type == "HOUSE1" || type == "HOUSE2" || type == "HOUSE3") {
        generate_house(vertices, 0);
    }
    else if (type == "AIRSHIP") {
        generate_airship(vertices);
    }
    else if (type == "PACKAGE") {
        generate_package(vertices);
    }

    if (vertices.empty()) {
        std::cout << "Warning: No vertices generated for " << type << std::endl;
        generate_package(vertices); 
    }

    computeTangents(vertices);

    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

    // Текстурные координаты
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, tangent));

    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, type));

    glBindVertexArray(0);

    unsigned int texture = 0;
    unsigned int normalMap = 0;

    if (!texturePath.empty()) {
        texture = load_texture(texturePath.c_str());
    }

    if (!normalPath.empty()) {
        normalMap = load_texture(normalPath.c_str());
    }

    return { VAO, texture, normalMap, (int)vertices.size(), color, type };
}

void generate_random_positions() {
    std::mt19937 rng(time(0));
    std::uniform_real_distribution<float> distPos(-200.0f, 200.0f);
    std::uniform_int_distribution<int> distType(0, 2);

    houses.clear();
    for (int i = 0; i < NUM_HOUSES; i++) {
        House house;
        house.position = glm::vec3(distPos(rng), 0, distPos(rng));
        house.hasPackage = false;
        house.houseType = distType(rng);
        houses.push_back(house);
    }

    treePositions.clear();
    for (int i = 0; i < NUM_TREES; i++) {
        TreeObject tree;
        tree.position = glm::vec3(distPos(rng), 0, distPos(rng));
        tree.windOffset = distPos(rng); 
        tree.treeHeight = 10.0f + (rand() % 50) / 10.0f; 
        treePositions.push_back(tree);
    }

    rockPositions.clear();
    for (int i = 0; i < NUM_ROCKS; i++) {
        rockPositions.push_back(glm::vec3(distPos(rng), 0, distPos(rng)));
    }

    totalHouses = NUM_HOUSES;
    deliveredPackages = 0;
}

void drop_package() {
    Package pkg;
    pkg.position = airshipPosition + glm::vec3(0, -10, 0);
    pkg.velocity = glm::vec3(0, -20.0f, 0);
    pkg.active = true;
    pkg.rotation = 0.0f;
    pkg.rotationSpeed = (rand() % 100) / 100.0f * 2.0f;

    packages.push_back(pkg);
}

void update_physics(float deltaTime) {
    windTime += deltaTime;

    // Обновление посылок
    for (auto& pkg : packages) {
        if (!pkg.active) continue;

        pkg.position += pkg.velocity * deltaTime;
        pkg.rotation += pkg.rotationSpeed * deltaTime;

        pkg.velocity.y -= 9.8f * deltaTime * 5.0f;

        // Проверка столкновения с землей
        if (pkg.position.y <= 5.0f) {
            pkg.active = false;

            for (auto& house : houses) {
                if (!house.hasPackage) {
                    float distance = glm::distance(
                        glm::vec3(pkg.position.x, 0, pkg.position.z),
                        house.position
                    );

                    if (distance < 20.0f) {
                        house.hasPackage = true;
                        deliveredPackages++;
                        break;
                    }
                }
            }
        }
    }

    packages.erase(
        std::remove_if(packages.begin(), packages.end(),
            [](const Package& p) { return !p.active; }),
        packages.end()
    );
}

void render_object(const GameObject& obj, const glm::mat4& model,
    bool useTexture = true, bool useNormalMap = false) {

    glBindVertexArray(obj.vao);

    if (useTexture && obj.texture != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, obj.texture);
    }

    if (useNormalMap && obj.normalMap != 0) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, obj.normalMap);
    }

    glDrawArrays(GL_TRIANGLES, 0, obj.vertexCount);
    glBindVertexArray(0);
}

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Airship Delivery Game", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return -1;
    }

    const GLubyte* renderer = glGetString(GL_RENDERER);
    const GLubyte* version = glGetString(GL_VERSION);
    std::cout << "Renderer: " << renderer << std::endl;
    std::cout << "OpenGL version: " << version << std::endl;

    glEnable(GL_DEPTH_TEST);
    glClearColor(0.53f, 0.81f, 0.92f, 1.0f); 

    std::cout << "Creating shader program..." << std::endl;
    unsigned int shaderProgram = CreateShaderProgram();
    if (shaderProgram == 0) {
        std::cerr << "Failed to create shader program" << std::endl;
        return -1;
    }

    glUseProgram(shaderProgram);
    std::cout << "Shader program created successfully" << std::endl;

    glUniform1i(glGetUniformLocation(shaderProgram, "texture0"), 0);
    glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 1);

    generate_random_positions();

    std::cout << "Creating game objects..." << std::endl;
    try {
        airship = create_object("AIRSHIP", "textures/metall.png", "textures/normalmap.png", glm::vec3(0.8f, 0.2f, 0.2f));
        field = create_object("FIELD", "textures/snow.png", "", glm::vec3(1.0f, 1.0f, 1.0f));
        tree = create_object("TREE", "textures/wood.png", "", glm::vec3(0.3f, 0.5f, 0.1f));
        rock = create_object("ROCK", "textures/stone.png", "", glm::vec3(0.5f, 0.5f, 0.5f));
        house1 = create_object("HOUSE1", "textures/wood.png", "", glm::vec3(0.7f, 0.5f, 0.3f));
        house2 = create_object("HOUSE2", "textures/wood.png", "", glm::vec3(0.8f, 0.4f, 0.3f));
        house3 = create_object("HOUSE3", "textures/wood.png", "", glm::vec3(0.6f, 0.3f, 0.2f));
        packageObj = create_object("PACKAGE", "", "", glm::vec3(0.9f, 0.8f, 0.1f));

        std::cout << "All objects created successfully" << std::endl;
        std::cout << "Airship normal map: " << (airship.normalMap != 0 ? "Loaded" : "Not loaded") << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Error creating objects: " << e.what() << std::endl;
        return -1;
    }

    camera.position = glm::vec3(0, 150, -100);
    camera.yaw = 0.0f;
    camera.pitch = -30.0f;

    int modelLoc = glGetUniformLocation(shaderProgram, "model");
    int viewLoc = glGetUniformLocation(shaderProgram, "view");
    int projectionLoc = glGetUniformLocation(shaderProgram, "projection");
    int lightDirLoc = glGetUniformLocation(shaderProgram, "lightDir");
    int useTextureLoc = glGetUniformLocation(shaderProgram, "useTexture");
    int useNormalMapLoc = glGetUniformLocation(shaderProgram, "useNormalMap");
    int baseColorLoc = glGetUniformLocation(shaderProgram, "baseColor");
    int timeLoc = glGetUniformLocation(shaderProgram, "time");
    int windEffectLoc = glGetUniformLocation(shaderProgram, "windEffect");
    int windStrengthLoc = glGetUniformLocation(shaderProgram, "windStrength");
    int windFrequencyLoc = glGetUniformLocation(shaderProgram, "windFrequency");
    int treeHeightLoc = glGetUniformLocation(shaderProgram, "treeHeight");
    int windOffsetLoc = glGetUniformLocation(shaderProgram, "windOffset");

    if (modelLoc == -1) std::cout << "Warning: model uniform not found" << std::endl;
    if (viewLoc == -1) std::cout << "Warning: view uniform not found" << std::endl;
    if (projectionLoc == -1) std::cout << "Warning: projection uniform not found" << std::endl;
    if (lightDirLoc == -1) std::cout << "Warning: lightDir uniform not found" << std::endl;
    if (baseColorLoc == -1) std::cout << "Warning: baseColor uniform not found" << std::endl;

    glm::mat4 projection = glm::perspective(glm::radians(45.0f), 1280.0f / 720.0f, 0.1f, 10000.0f);
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));

    glm::vec3 lightDir = glm::normalize(glm::vec3(0.5f, -1.0f, 0.5f));
    glUniform3fv(lightDirLoc, 1, glm::value_ptr(lightDir));

    double lastTime = glfwGetTime();
    std::cout << "Entering main loop..." << std::endl;

    while (!glfwWindowShouldClose(window)) {
        double currentTime = glfwGetTime();
        float deltaTime = float(currentTime - lastTime);
        lastTime = currentTime;

        update_physics(deltaTime);

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, true);
        }

        float moveSpeed = airshipSpeed * deltaTime;

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
            airshipPosition.z -= moveSpeed * cos(glm::radians(airshipRotation));
            airshipPosition.x -= moveSpeed * sin(glm::radians(airshipRotation));
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
            airshipPosition.z += moveSpeed * cos(glm::radians(airshipRotation));
            airshipPosition.x += moveSpeed * sin(glm::radians(airshipRotation));
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
            airshipRotation += 60.0f * deltaTime;
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
            airshipRotation -= 60.0f * deltaTime;
        }
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            airshipPosition.y += moveSpeed;
        }
        if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
            airshipPosition.y -= moveSpeed;
        }

        if (airshipPosition.y < 30.0f) airshipPosition.y = 30.0f;
        if (airshipPosition.y > 300.0f) airshipPosition.y = 300.0f;

        static bool spacePressed = false;
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS && !spacePressed) {
            if (packages.size() < NUM_PACKAGES_MAX) {
                drop_package();
                std::cout << "Package dropped!" << std::endl;
            }
            spacePressed = true;
        }
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_RELEASE) {
            spacePressed = false;
        }

        if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS && !cPressed) {
            aimMode = !aimMode;
            cPressed = true;
        }
        if (glfwGetKey(window, GLFW_KEY_C) == GLFW_RELEASE) {
            cPressed = false;
        }

        if (aimMode) {
            camera.position = airshipPosition + glm::vec3(0, -15, 0);
            camera.yaw = airshipRotation;
            camera.pitch = -10.0f;
        }
        else {
            float camDistance = 80.0f;
            float camHeight = 40.0f;

            camera.position = airshipPosition +
                glm::vec3(
                    sin(glm::radians(airshipRotation)) * camDistance,
                    camHeight,
                    cos(glm::radians(airshipRotation)) * camDistance
                );
            camera.yaw = airshipRotation + 180.0f;
            camera.pitch = -25.0f;
        }

        glm::mat4 view = camera.GetView();
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

        glUniform1f(timeLoc, (float)currentTime);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUniform1i(useTextureLoc, 1);
        glUniform1i(useNormalMapLoc, 0);
        glUniform1i(windEffectLoc, 0);
        glUniform3fv(baseColorLoc, 1, glm::value_ptr(field.baseColor));

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::scale(model, glm::vec3(1.0f, 1.0f, 1.0f));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        render_object(field, model);

        glUniform1i(windEffectLoc, 1);  
        glUniform1f(windStrengthLoc, 0.2f);  
        glUniform1f(windFrequencyLoc, 1.8f); 
        glUniform1i(useTextureLoc, 1);
        glUniform3fv(baseColorLoc, 1, glm::value_ptr(tree.baseColor));

        for (const auto& treeObj : treePositions) {
            model = glm::mat4(1.0f);
            model = glm::translate(model, treeObj.position);
            model = glm::scale(model, glm::vec3(1.0f, treeObj.treeHeight / 12.0f, 1.0f)); 
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

            glUniform1f(treeHeightLoc, treeObj.treeHeight);
            glUniform1f(windOffsetLoc, treeObj.windOffset);

            render_object(tree, model);
        }

        glUniform1i(windEffectLoc, 0);  

        glUniform3fv(baseColorLoc, 1, glm::value_ptr(rock.baseColor));

        for (const auto& pos : rockPositions) {
            model = glm::mat4(1.0f);
            model = glm::translate(model, pos);
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
            render_object(rock, model);
        }

        glUniform1i(useTextureLoc, 1);
        for (const auto& house : houses) {
            GameObject* houseObj;
            switch (house.houseType) {
            case 0: houseObj = &house1; break;
            case 1: houseObj = &house2; break;
            case 2: houseObj = &house3; break;
            default: houseObj = &house1;
            }

            glm::vec3 color = houseObj->baseColor;
            if (!house.hasPackage) {
                color = glm::mix(color, glm::vec3(1.0f, 0.0f, 0.0f), 0.3f);
            }

            glUniform3fv(baseColorLoc, 1, glm::value_ptr(color));

            model = glm::mat4(1.0f);
            model = glm::translate(model, house.position);
            model = glm::scale(model, glm::vec3(1.0f, 1.0f, 1.0f));
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
            render_object(*houseObj, model);
        }

        glUniform1i(useNormalMapLoc, 1);  
        glUniform1i(useTextureLoc, 1);
        glUniform3fv(baseColorLoc, 1, glm::value_ptr(airship.baseColor));

        model = glm::mat4(1.0f);
        model = glm::translate(model, airshipPosition);
        model = glm::rotate(model, glm::radians(airshipRotation), glm::vec3(0, 1, 0));
        model = glm::scale(model, glm::vec3(1.0f, 1.0f, 1.0f));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        render_object(airship, model, true, true);  

        glUniform1i(useNormalMapLoc, 0);
        glUniform1i(useTextureLoc, 0);
        glUniform3fv(baseColorLoc, 1, glm::value_ptr(packageObj.baseColor));

        for (const auto& pkg : packages) {
            if (pkg.active) {
                model = glm::mat4(1.0f);
                model = glm::translate(model, pkg.position);
                model = glm::rotate(model, pkg.rotation, glm::vec3(0, 1, 0));
                model = glm::scale(model, glm::vec3(1.0f, 1.0f, 1.0f));
                glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
                render_object(packageObj, model, false);
            }
        }

        GLenum error = glGetError();
        if (error != GL_NO_ERROR) {
            std::cout << "OpenGL error: " << error << std::endl;
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    std::cout << "Program terminated successfully" << std::endl;
    return 0;
}