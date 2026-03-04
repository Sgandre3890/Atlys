// Model.cpp – tinygltf-based GLTF/GLB loader
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"

#include "model.h"
#include "shader.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

#include <iostream>
#include <stdexcept>
#include <cstring>

// ── Helpers ───────────────────────────────────────────────────────────────────

static glm::mat4 nodeLocalTransform(const tinygltf::Node& n) {
    if (!n.matrix.empty()) {
        // Column-major matrix stored in the node
        glm::mat4 m;
        for (int col = 0; col < 4; ++col)
            for (int row = 0; row < 4; ++row)
                m[col][row] = static_cast<float>(n.matrix[col * 4 + row]);
        return m;
    }

    glm::mat4 T(1.0f), R(1.0f), S(1.0f);
    if (!n.translation.empty())
        T = glm::translate(glm::mat4(1.0f),
            glm::vec3((float)n.translation[0],
                      (float)n.translation[1],
                      (float)n.translation[2]));
    if (!n.rotation.empty()) {
        glm::quat q((float)n.rotation[3],   // w
                    (float)n.rotation[0],   // x
                    (float)n.rotation[1],   // y
                    (float)n.rotation[2]);  // z
        R = glm::toMat4(q);
    }
    if (!n.scale.empty())
        S = glm::scale(glm::mat4(1.0f),
            glm::vec3((float)n.scale[0],
                      (float)n.scale[1],
                      (float)n.scale[2]));
    return T * R * S;
}

// ── Public API ────────────────────────────────────────────────────────────────

bool Model::load(const std::string& path) {
    tinygltf::TinyGLTF loader;
    tinygltf::Model    gltfModel;
    std::string        err, warn;

    bool ok;
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".glb")
        ok = loader.LoadBinaryFromFile(&gltfModel, &err, &warn, path);
    else
        ok = loader.LoadASCIIFromFile (&gltfModel, &err, &warn, path);

    if (!warn.empty()) std::cerr << "[gltf] WARNING: " << warn << "\n";
    if (!err.empty())  std::cerr << "[gltf] ERROR:   " << err  << "\n";
    if (!ok) return false;

    // Walk the default scene (or scene 0)
    int sceneIdx = gltfModel.defaultScene >= 0 ? gltfModel.defaultScene : 0;
    const auto& scene = gltfModel.scenes[sceneIdx];

    for (int nodeIdx : scene.nodes)
        processNode(gltfModel, nodeIdx, glm::mat4(1.0f));

    std::cout << "[gltf] Loaded \"" << path << "\" — "
              << primitives.size() << " primitive(s), "
              << nodes.size()      << " node(s)\n";
    return true;
}

void Model::destroy() {
    for (auto& p : primitives) {
        glDeleteVertexArrays(1, &p.vao);
        glDeleteBuffers(1, &p.vbo);
        if (p.ebo) glDeleteBuffers(1, &p.ebo);
    }
    for (auto& [_, tex] : m_texCache)
        glDeleteTextures(1, &tex);
    primitives.clear();
    nodes.clear();
    m_texCache.clear();
}

// ── Draw ──────────────────────────────────────────────────────────────────────

void Model::draw(Shader& shader) const {
    for (const auto& node : nodes)
        drawNode(node, shader, glm::mat4(1.0f));
}

void Model::drawNode(const Node& node, Shader& shader, const glm::mat4& parent) const {
    glm::mat4 world = parent * node.transform;
    shader.setMat4("uModel", world);

    for (int pi : node.primitiveIndices) {
        const Primitive& prim = primitives[pi];

        shader.setVec3("uBaseColor", glm::vec3(prim.baseColorFactor));
        shader.setBool("uHasTexture", prim.hasTexture);

        if (prim.hasTexture) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, prim.baseColorTex);
            shader.setInt("uAlbedoTex", 0);
        }

        glBindVertexArray(prim.vao);
        if (prim.hasIndices)
            glDrawElements(GL_TRIANGLES, prim.indexCount, prim.indexType, nullptr);
        else
            glDrawArrays(GL_TRIANGLES, 0, prim.vertexCount);
        glBindVertexArray(0);
    }

    for (int child : node.children)
        drawNode(nodes[child], shader, world);
}

// ── Scene graph traversal ─────────────────────────────────────────────────────

void Model::processNode(const tinygltf::Model& gltfModel, int nodeIdx, const glm::mat4& parentTransform) {
    const tinygltf::Node& gltfNode = gltfModel.nodes[nodeIdx];
    glm::mat4 localT = nodeLocalTransform(gltfNode);

    Node node;
    node.transform = parentTransform * localT;

    if (gltfNode.mesh >= 0) {
        int startPrim = (int)primitives.size();
        processMesh(gltfModel, gltfModel.meshes[gltfNode.mesh]);
        for (int i = startPrim; i < (int)primitives.size(); ++i)
            node.primitiveIndices.push_back(i);
    }

    int myIndex = (int)nodes.size();
    nodes.push_back(node);

    for (int child : gltfNode.children) {
        int childIdx = (int)nodes.size();
        nodes[myIndex].children.push_back(childIdx);
        processNode(gltfModel, child, node.transform);
    }
}

int Model::processMesh(const tinygltf::Model& gltfModel, const tinygltf::Mesh& mesh) {
    int first = (int)primitives.size();
    for (const auto& prim : mesh.primitives)
        buildPrimitive(gltfModel, prim);
    return first;
}

// ── Primitive build (interleaved VBO) ─────────────────────────────────────────

int Model::buildPrimitive(const tinygltf::Model& model, const tinygltf::Primitive& prim) {
    Primitive out;

    // ── Accessors ──────────────────────────────────────────────────────────────
    auto findAttr = [&](const std::string& name) -> const tinygltf::Accessor* {
        auto it = prim.attributes.find(name);
        if (it == prim.attributes.end()) return nullptr;
        return &model.accessors[it->second];
    };

    const tinygltf::Accessor* posAcc    = findAttr("POSITION");
    const tinygltf::Accessor* normAcc   = findAttr("NORMAL");
    const tinygltf::Accessor* uvAcc     = findAttr("TEXCOORD_0");

    if (!posAcc) {
        std::cerr << "[gltf] Primitive has no POSITION, skipping.\n";
        return -1;
    }

    size_t vertCount = posAcc->count;

    // Helper to get a typed pointer to accessor data
    auto getDataPtr = [&](const tinygltf::Accessor& acc) -> const unsigned char* {
        const tinygltf::BufferView& bv = model.bufferViews[acc.bufferView];
        return model.buffers[bv.buffer].data.data() + bv.byteOffset + acc.byteOffset;
    };

    auto getStride = [&](const tinygltf::Accessor& acc, size_t elemSize) -> size_t {
        const tinygltf::BufferView& bv = model.bufferViews[acc.bufferView];
        return bv.byteStride ? bv.byteStride : elemSize;
    };

    // ── Build interleaved buffer: [pos(3f) | norm(3f) | uv(2f)] ───────────────
    struct Vertex { float pos[3]; float norm[3]; float uv[2]; };
    std::vector<Vertex> vertices(vertCount);

    // Positions
    {
        const unsigned char* ptr = getDataPtr(*posAcc);
        size_t stride = getStride(*posAcc, sizeof(float)*3);
        for (size_t i = 0; i < vertCount; ++i) {
            const float* f = reinterpret_cast<const float*>(ptr + i * stride);
            vertices[i].pos[0] = f[0];
            vertices[i].pos[1] = f[1];
            vertices[i].pos[2] = f[2];
            // Update AABB
            aabbMin = glm::min(aabbMin, glm::vec3(f[0],f[1],f[2]));
            aabbMax = glm::max(aabbMax, glm::vec3(f[0],f[1],f[2]));
        }
    }

    // Normals (or default up)
    if (normAcc) {
        const unsigned char* ptr = getDataPtr(*normAcc);
        size_t stride = getStride(*normAcc, sizeof(float)*3);
        for (size_t i = 0; i < vertCount; ++i) {
            const float* f = reinterpret_cast<const float*>(ptr + i * stride);
            vertices[i].norm[0] = f[0];
            vertices[i].norm[1] = f[1];
            vertices[i].norm[2] = f[2];
        }
    } else {
        for (auto& v : vertices) { v.norm[0]=0; v.norm[1]=1; v.norm[2]=0; }
    }

    // UVs
    if (uvAcc) {
        const unsigned char* ptr = getDataPtr(*uvAcc);
        size_t stride = getStride(*uvAcc, sizeof(float)*2);
        for (size_t i = 0; i < vertCount; ++i) {
            const float* f = reinterpret_cast<const float*>(ptr + i * stride);
            vertices[i].uv[0] = f[0];
            vertices[i].uv[1] = f[1];
        }
    } else {
        for (auto& v : vertices) { v.uv[0] = v.uv[1] = 0.0f; }
    }

    // ── Upload VBO ─────────────────────────────────────────────────────────────
    glGenVertexArrays(1, &out.vao);
    glGenBuffers(1, &out.vbo);
    glBindVertexArray(out.vao);

    glBindBuffer(GL_ARRAY_BUFFER, out.vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(vertCount * sizeof(Vertex)),
                 vertices.data(), GL_STATIC_DRAW);

    // layout(location=0) vec3 aPos
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, pos));
    // layout(location=1) vec3 aNormal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, norm));
    // layout(location=2) vec2 aUV
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, uv));

    out.vertexCount = (GLsizei)vertCount;

    // ── Index buffer ───────────────────────────────────────────────────────────
    if (prim.indices >= 0) {
        const tinygltf::Accessor& idxAcc = model.accessors[prim.indices];
        const tinygltf::BufferView& bv   = model.bufferViews[idxAcc.bufferView];
        const unsigned char* idxPtr      = model.buffers[bv.buffer].data.data()
                                         + bv.byteOffset + idxAcc.byteOffset;

        // Normalise everything to uint32 for simplicity
        std::vector<uint32_t> indices(idxAcc.count);
        if (idxAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
            const uint16_t* s = reinterpret_cast<const uint16_t*>(idxPtr);
            for (size_t i = 0; i < idxAcc.count; ++i) indices[i] = s[i];
        } else if (idxAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
            for (size_t i = 0; i < idxAcc.count; ++i) indices[i] = idxPtr[i];
        } else { // UNSIGNED_INT
            std::memcpy(indices.data(), idxPtr, idxAcc.count * sizeof(uint32_t));
        }

        glGenBuffers(1, &out.ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, out.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     (GLsizeiptr)(indices.size() * sizeof(uint32_t)),
                     indices.data(), GL_STATIC_DRAW);

        out.indexCount  = (GLsizei)indices.size();
        out.indexType   = GL_UNSIGNED_INT;
        out.hasIndices  = true;
    }

    glBindVertexArray(0);

    // ── Material ───────────────────────────────────────────────────────────────
    if (prim.material >= 0) {
        const auto& mat = model.materials[prim.material];
        const auto& pbr = mat.pbrMetallicRoughness;
        auto& cf = pbr.baseColorFactor;
        if (cf.size() == 4)
            out.baseColorFactor = glm::vec4((float)cf[0],(float)cf[1],(float)cf[2],(float)cf[3]);

        if (pbr.baseColorTexture.index >= 0) {
            out.baseColorTex = loadTexture(model, pbr.baseColorTexture.index);
            out.hasTexture   = (out.baseColorTex != 0);
        }
    }

    primitives.push_back(out);
    return (int)primitives.size() - 1;
}

// ── Texture upload ────────────────────────────────────────────────────────────

GLuint Model::loadTexture(const tinygltf::Model& model, int texIndex) {
    auto it = m_texCache.find(texIndex);
    if (it != m_texCache.end()) return it->second;

    const tinygltf::Texture& tex = model.textures[texIndex];
    if (tex.source < 0) return 0;
    const tinygltf::Image& img = model.images[tex.source];

    if (img.image.empty()) return 0;

    GLenum fmt = GL_RGBA;
    GLenum ifmt = GL_RGBA8;
    if      (img.component == 1){ fmt = GL_RED;  ifmt = GL_R8;   }
    else if (img.component == 3){ fmt = GL_RGB;  ifmt = GL_RGB8; }

    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, (GLint)ifmt,
                 img.width, img.height, 0,
                 fmt, GL_UNSIGNED_BYTE, img.image.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    m_texCache[texIndex] = id;
    return id;
}