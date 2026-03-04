#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <unordered_map>

// Forward-declare so we don't pull in all of tinygltf in every TU
namespace tinygltf { class Model; struct Mesh; struct Primitive; }

// ── Per-primitive GPU data ─────────────────────────────────────────────────────
struct Primitive {
    GLuint vao  = 0;
    GLuint vbo  = 0;   // interleaved: pos(3) + normal(3) + uv(2)
    GLuint ebo  = 0;
    GLsizei indexCount   = 0;
    GLenum  indexType    = GL_UNSIGNED_INT;
    bool    hasIndices   = false;
    GLsizei vertexCount  = 0;

    // Material (PBR base colour)
    glm::vec4 baseColorFactor = glm::vec4(1.0f);
    GLuint    baseColorTex    = 0;   // 0 = no texture
    bool      hasTexture      = false; 
};

// ── Scene node ─────────────────────────────────────────────────────────────────
struct Node {
    glm::mat4         transform = glm::mat4(1.0f);
    std::vector<int>  children;
    std::vector<int>  primitiveIndices;   // indices into Model::primitives
};

// ── Top-level model ────────────────────────────────────────────────────────────
class Model {
public:
    std::vector<Primitive> primitives;
    std::vector<Node>      nodes;
    int                    rootNode = -1;  // -1 = draw all nodes

    // Tight bounding box (world space at load time)
    glm::vec3 aabbMin = glm::vec3( 1e30f);
    glm::vec3 aabbMax = glm::vec3(-1e30f);

    bool load(const std::string& path);
    void draw(class Shader& shader) const;
    void destroy();

private:
    std::unordered_map<int,GLuint> m_texCache;  // gltf texIndex -> GL texID

    void processNode(const tinygltf::Model& model, int nodeIdx, const glm::mat4& parentTransform);
    int  processMesh(const tinygltf::Model& model, const tinygltf::Mesh& mesh);
    int  buildPrimitive(const tinygltf::Model& model, const tinygltf::Primitive& prim);
    GLuint loadTexture(const tinygltf::Model& model, int texIndex);

    void drawNode(const Node& node, class Shader& shader, const glm::mat4& parent) const;
};
