#pragma once

// ─────────────────────────────────────────────────────────────────────────────
//  ModelLoader.h  –  Simple model loader using Assimp
//
//  Public API:
//    bool loadModel   (path, Model&)
//    void drawModel   (Model&, shader, parentTransform)
//    void destroyModel(Model&)
//
//  Vertex attribute locations:  0=position  1=normal  2=texcoord
//  Uniforms: u_Model, u_View, u_Projection, u_CameraPos, u_LightDir,
//            u_AlbedoTex, u_HasAlbedo, u_BaseColor
// ─────────────────────────────────────────────────────────────────────────────

#include <vector>
#include <string>
#include <unordered_map>
#include <cstdio>

#include <glad/gl.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// ─────────────────────────────────────────────────────────────────────────────

struct Mesh {
    GLuint    vao        = 0;
    GLuint    vbo        = 0;
    GLuint    ebo        = 0;
    int       indexCount = 0;
    GLuint    albedoTex  = 0;
    glm::vec4 baseColor  = glm::vec4(1.0f);
};

struct Model {
    std::vector<Mesh> meshes;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Internals
// ─────────────────────────────────────────────────────────────────────────────

struct Vertex { glm::vec3 position, normal; glm::vec2 texcoord; };

// Cache so the same texture file isn't uploaded to the GPU twice
static std::unordered_map<std::string, GLuint> s_texCache;

static GLuint uploadPixels(const unsigned char* px, int w, int h, int ch) {
    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    // Use sRGB internal format so the GPU linearises the texture for us –
    // this prevents the double-gamma issue (washed-out or too-dark colours).
    GLenum internal = (ch == 4) ? GL_SRGB_ALPHA : GL_SRGB;
    GLenum src      = (ch == 4) ? GL_RGBA        : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, internal, w, h, 0, src, GL_UNSIGNED_BYTE, px);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    return id;
}

static GLuint loadTexture(const aiString& aiPath, const aiScene* scene,
                          const std::string& modelDir) {
    const char* path = aiPath.C_Str(); // always C_Str() – avoids the 4-char truncation bug

    // ── Embedded texture (GLB: "*0", "*1", …) ─────────────────────────────
    if (path[0] == '*') {
        int idx = atoi(path + 1);
        const aiTexture* t = scene->mTextures[idx];
        int w, h, ch;
        unsigned char* px;
        if (t->mHeight == 0) {
            // Compressed PNG/JPG blob – let stb decode it
            px = stbi_load_from_memory(
                reinterpret_cast<const unsigned char*>(t->pcData),
                (int)t->mWidth, &w, &h, &ch, 0);
        } else {
            // Raw ARGB8888
            w = t->mWidth; h = t->mHeight; ch = 4;
            px = new unsigned char[w * h * 4];
            for (int i = 0; i < w * h; ++i) {
                px[i*4+0] = t->pcData[i].r; px[i*4+1] = t->pcData[i].g;
                px[i*4+2] = t->pcData[i].b; px[i*4+3] = t->pcData[i].a;
            }
        }
        if (!px) { printf("Failed to decode embedded texture %d\n", idx); return 0; }
        GLuint id = uploadPixels(px, w, h, ch);
        stbi_image_free(px);
        return id;
    }

    // ── External file ──────────────────────────────────────────────────────
    // Try the path as-is first, then prepend the model directory.
    // This handles both absolute paths and relative ones like "textures/diffuse.png".
    std::string candidates[] = {
        path,
        modelDir + "/" + path,
    };

    for (const std::string& fullPath : candidates) {
        auto it = s_texCache.find(fullPath);
        if (it != s_texCache.end()) return it->second; // already uploaded

        int w, h, ch;
        // stbi_set_flip_vertically_on_load handled per-format below
        unsigned char* px = stbi_load(fullPath.c_str(), &w, &h, &ch, 0);
        if (!px) continue;

        GLuint id = uploadPixels(px, w, h, ch);
        stbi_image_free(px);
        s_texCache[fullPath] = id;
        return id;
    }

    printf("Could not find texture: %s\n", path);
    return 0;
}

static Mesh buildMesh(const aiMesh* ai, const aiScene* scene,
                      const std::string& modelDir) {
    // Vertices
    std::vector<Vertex> verts(ai->mNumVertices);
    for (unsigned i = 0; i < ai->mNumVertices; ++i) {
        verts[i].position = { ai->mVertices[i].x, ai->mVertices[i].y, ai->mVertices[i].z };
        verts[i].normal   = ai->HasNormals()
                            ? glm::vec3(ai->mNormals[i].x, ai->mNormals[i].y, ai->mNormals[i].z)
                            : glm::vec3(0, 1, 0);
        verts[i].texcoord = ai->HasTextureCoords(0)
                            ? glm::vec2(ai->mTextureCoords[0][i].x, ai->mTextureCoords[0][i].y)
                            : glm::vec2(0, 0);
    }

    // Indices
    std::vector<uint32_t> indices;
    indices.reserve(ai->mNumFaces * 3);
    for (unsigned i = 0; i < ai->mNumFaces; ++i)
        for (unsigned j = 0; j < ai->mFaces[i].mNumIndices; ++j)
            indices.push_back(ai->mFaces[i].mIndices[j]);

    // Upload
    Mesh mesh;
    mesh.indexCount = (int)indices.size();
    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glGenBuffers(1, &mesh.ebo);
    glBindVertexArray(mesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vertex), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));  glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texcoord));  glEnableVertexAttribArray(2);
    glBindVertexArray(0);

    // Material
    if (ai->mMaterialIndex >= 0) {
        const aiMaterial* mat = scene->mMaterials[ai->mMaterialIndex];

        aiColor4D col(1, 1, 1, 1);
        mat->Get(AI_MATKEY_COLOR_DIFFUSE, col);
        mesh.baseColor = { col.r, col.g, col.b, col.a };

        // Try PBR base colour first (GLB/GLTF), fall back to classic diffuse (OBJ/FBX)
        aiString texPath;
        if (mat->GetTexture(aiTextureType_BASE_COLOR, 0, &texPath) == AI_SUCCESS ||
            mat->GetTexture(aiTextureType_DIFFUSE,    0, &texPath) == AI_SUCCESS) {
            mesh.albedoTex = loadTexture(texPath, scene, modelDir);
        }
    }

    return mesh;
}

static void processNode(const aiNode* node, const aiScene* scene,
                        const std::string& dir, Model& out) {
    for (unsigned i = 0; i < node->mNumMeshes; ++i)
        out.meshes.push_back(buildMesh(scene->mMeshes[node->mMeshes[i]], scene, dir));
    for (unsigned i = 0; i < node->mNumChildren; ++i)
        processNode(node->mChildren[i], scene, dir, out);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Public API
// ─────────────────────────────────────────────────────────────────────────────

inline bool loadModel(const std::string& path, Model& out) {
    s_texCache.clear();

    // Flip UVs vertically for formats that store V=0 at the top (FBX, DAE, GLB).
    // OBJ already matches OpenGL convention so no flip is needed.
    std::string ext = path.substr(path.find_last_of('.') + 1);
    for (char& c : ext) c = (char)tolower((unsigned char)c);
    stbi_set_flip_vertically_on_load(ext != "obj" ? 1 : 0);

    Assimp::Importer imp;
    const aiScene* scene = imp.ReadFile(path,
        aiProcess_Triangulate           |
        aiProcess_GenSmoothNormals      |
        aiProcess_JoinIdenticalVertices |
        aiProcess_PreTransformVertices);  // bakes node transforms – stops meshes scattering

    if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode) {
        printf("Assimp error: %s\n", imp.GetErrorString());
        return false;
    }

    std::string dir = path.substr(0, path.find_last_of("/\\"));
    processNode(scene->mRootNode, scene, dir, out);
    printf("Loaded '%s': %zu meshes\n", path.c_str(), out.meshes.size());
    return true;
}

inline void drawModel(const Model& model, GLuint shader,
                      const glm::mat4& transform = glm::mat4(1.0f)) {
    glUniformMatrix4fv(glGetUniformLocation(shader, "u_Model"), 1, GL_FALSE, glm::value_ptr(transform));
    for (const Mesh& m : model.meshes) {
        glUniform4fv(glGetUniformLocation(shader, "u_BaseColor"), 1, glm::value_ptr(m.baseColor));
        glUniform1i(glGetUniformLocation(shader, "u_HasAlbedo"), m.albedoTex ? 1 : 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m.albedoTex);
        glUniform1i(glGetUniformLocation(shader, "u_AlbedoTex"), 0);
        glBindVertexArray(m.vao);
        glDrawElements(GL_TRIANGLES, m.indexCount, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
    }
}

inline void destroyModel(Model& model) {
    for (Mesh& m : model.meshes) {
        glDeleteVertexArrays(1, &m.vao);
        glDeleteBuffers(1, &m.vbo);
        glDeleteBuffers(1, &m.ebo);
        if (m.albedoTex) glDeleteTextures(1, &m.albedoTex);
    }
    model.meshes.clear();
    s_texCache.clear();
}
