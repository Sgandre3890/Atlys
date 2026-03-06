#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <unordered_map>

namespace tinygltf { class Model; struct Mesh; struct Primitive; }

// ── Per-primitive GPU data ─────────────────────────────────────────────────────
struct Primitive {
    GLuint vao = 0;
    GLuint vbo = 0;   // interleaved: pos(3) + norm(3) + tan(4) + uv0(2) + uv1(2) + color(4)
    GLuint ebo = 0;
    GLsizei indexCount  = 0;
    GLenum  indexType   = GL_UNSIGNED_INT;
    bool    hasIndices  = false;
    GLsizei vertexCount = 0;
    GLenum  drawMode    = GL_TRIANGLES;  // supports POINTS, LINES, TRIANGLES etc.

    // PBR material
    glm::vec4 baseColorFactor       = glm::vec4(1.0f);
    glm::vec3 emissiveFactor        = glm::vec3(0.0f);
    float     metallicFactor        = 1.0f;
    float     roughnessFactor       = 1.0f;
    float     occlusionStrength     = 1.0f;
    float     normalScale           = 1.0f;

    GLuint baseColorTex    = 0;
    GLuint normalTex       = 0;
    GLuint metallicRoughTex= 0;
    GLuint emissiveTex     = 0;
    GLuint occlusionTex    = 0;

    bool hasBaseColorTex    = false;
    bool hasNormalTex       = false;
    bool hasMetallicRoughTex= false;
    bool hasEmissiveTex     = false;
    bool hasOcclusionTex    = false;
    bool hasVertexColors    = false;
    bool hasTangents        = false;

    // Alpha / culling
    bool  doubleSided   = false;
    int   alphaMode     = 0;   // 0=OPAQUE 1=MASK 2=BLEND
    float alphaCutoff   = 0.5f;
};

// ── Scene node ─────────────────────────────────────────────────────────────────
struct Node {
    glm::mat4        transform = glm::mat4(1.0f);
    std::vector<int> children;
    std::vector<int> primitiveIndices;
};

// ── Top-level model ────────────────────────────────────────────────────────────
class Model {
public:
    std::vector<Primitive> primitives;
    std::vector<Node>      nodes;

    glm::vec3 aabbMin = glm::vec3( 1e30f);
    glm::vec3 aabbMax = glm::vec3(-1e30f);

    bool load(const std::string& path);
    void draw(class Shader& shader) const;
    void destroy();

private:
    std::unordered_map<int,GLuint> m_texCache;

    void   processNode    (const tinygltf::Model&, int nodeIdx, const glm::mat4& parent);
    void   processMesh    (const tinygltf::Model&, const tinygltf::Mesh&);
    int    buildPrimitive (const tinygltf::Model&, const tinygltf::Primitive&);
    GLuint loadTexture    (const tinygltf::Model&, int texIndex);

    void drawNode(const Node&, class Shader&, const glm::mat4& parent) const;
};