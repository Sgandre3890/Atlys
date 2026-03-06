// model.cpp – tinygltf GLTF/GLB loader with full PBR material support
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
#include <fstream>
#include <cstring>
#include <cmath>

// ── Node local transform ──────────────────────────────────────────────────────
static glm::mat4 nodeLocalTransform(const tinygltf::Node& n) {
    if (!n.matrix.empty()) {
        glm::mat4 m;
        for (int col = 0; col < 4; ++col)
            for (int row = 0; row < 4; ++row)
                m[col][row] = (float)n.matrix[col*4+row];
        return m;
    }
    glm::mat4 T(1), R(1), S(1);
    if (!n.translation.empty())
        T = glm::translate(glm::mat4(1),
            glm::vec3((float)n.translation[0],(float)n.translation[1],(float)n.translation[2]));
    if (!n.rotation.empty()) {
        glm::quat q((float)n.rotation[3],(float)n.rotation[0],
                    (float)n.rotation[1],(float)n.rotation[2]);
        R = glm::toMat4(q);
    }
    if (!n.scale.empty())
        S = glm::scale(glm::mat4(1),
            glm::vec3((float)n.scale[0],(float)n.scale[1],(float)n.scale[2]));
    return T * R * S;
}

// ── Accessor helpers ──────────────────────────────────────────────────────────
static const unsigned char* dataPtr(const tinygltf::Model& m, const tinygltf::Accessor& acc) {
    const auto& bv = m.bufferViews[acc.bufferView];
    return m.buffers[bv.buffer].data.data() + bv.byteOffset + acc.byteOffset;
}
static size_t byteStride(const tinygltf::Model& m, const tinygltf::Accessor& acc, size_t elemBytes) {
    const auto& bv = m.bufferViews[acc.bufferView];
    return bv.byteStride ? bv.byteStride : elemBytes;
}

// Read a float2 accessor, handling FLOAT and also UNSIGNED_BYTE/SHORT normalised
static glm::vec2 readVec2(const unsigned char* base, size_t stride, size_t i, int compType) {
    const unsigned char* p = base + i*stride;
    if (compType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
        auto f = reinterpret_cast<const float*>(p);
        return {f[0], f[1]};
    } else if (compType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
        return {p[0]/255.f, p[1]/255.f};
    } else if (compType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
        auto s = reinterpret_cast<const uint16_t*>(p);
        return {s[0]/65535.f, s[1]/65535.f};
    }
    return {0,0};
}

static glm::vec3 readVec3(const unsigned char* base, size_t stride, size_t i) {
    auto f = reinterpret_cast<const float*>(base + i*stride);
    return {f[0],f[1],f[2]};
}
static glm::vec4 readVec4(const unsigned char* base, size_t stride, size_t i, int compType) {
    const unsigned char* p = base + i*stride;
    if (compType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
        auto f = reinterpret_cast<const float*>(p);
        return {f[0],f[1],f[2],f[3]};
    } else if (compType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
        return {p[0]/255.f,p[1]/255.f,p[2]/255.f,p[3]/255.f};
    } else if (compType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
        auto s = reinterpret_cast<const uint16_t*>(p);
        return {s[0]/65535.f,s[1]/65535.f,s[2]/65535.f,s[3]/65535.f};
    }
    return {1,1,1,1};
}

// ── Generate flat normals when mesh has none ──────────────────────────────────
static void generateFlatNormals(std::vector<glm::vec3>& norms,
                                 const std::vector<glm::vec3>& pos,
                                 const std::vector<uint32_t>& idx)
{
    norms.assign(pos.size(), glm::vec3(0));
    if (!idx.empty()) {
        for (size_t i = 0; i+2 < idx.size(); i+=3) {
            glm::vec3 e1 = pos[idx[i+1]]-pos[idx[i]];
            glm::vec3 e2 = pos[idx[i+2]]-pos[idx[i]];
            glm::vec3 n  = glm::normalize(glm::cross(e1,e2));
            norms[idx[i]] += n; norms[idx[i+1]] += n; norms[idx[i+2]] += n;
        }
        for (auto& n : norms) if(glm::length(n)>1e-6f) n = glm::normalize(n);
    } else {
        for (size_t i = 0; i+2 < pos.size(); i+=3) {
            glm::vec3 n = glm::normalize(glm::cross(pos[i+1]-pos[i],pos[i+2]-pos[i]));
            norms[i]=norms[i+1]=norms[i+2]=n;
        }
    }
}

// ── Generate mikktspace-style tangents when mesh has none ─────────────────────
static void generateTangents(std::vector<glm::vec4>& tans,
                               const std::vector<glm::vec3>& pos,
                               const std::vector<glm::vec3>& norms,
                               const std::vector<glm::vec2>& uvs,
                               const std::vector<uint32_t>& idx)
{
    size_t N = pos.size();
    tans.assign(N, glm::vec4(1,0,0,1));
    if (uvs.empty()) return;

    std::vector<glm::vec3> T(N,glm::vec3(0)), B(N,glm::vec3(0));
    auto process = [&](uint32_t i0, uint32_t i1, uint32_t i2){
        glm::vec3 e1=pos[i1]-pos[i0], e2=pos[i2]-pos[i0];
        glm::vec2 d1=uvs[i1]-uvs[i0], d2=uvs[i2]-uvs[i0];
        float r = d1.x*d2.y - d2.x*d1.y;
        if (std::abs(r)<1e-8f) return;
        float f=1.f/r;
        glm::vec3 t=f*(d2.y*e1-d1.y*e2);
        glm::vec3 b=f*(-d2.x*e1+d1.x*e2);
        T[i0]+=t; T[i1]+=t; T[i2]+=t;
        B[i0]+=b; B[i1]+=b; B[i2]+=b;
    };
    if (!idx.empty())
        for (size_t i=0;i+2<idx.size();i+=3) process(idx[i],idx[i+1],idx[i+2]);
    else
        for (size_t i=0;i+2<pos.size();i+=3) process((uint32_t)i,(uint32_t)(i+1),(uint32_t)(i+2));

    for (size_t i=0;i<N;i++){
        glm::vec3 n=norms[i];
        glm::vec3 t=glm::normalize(T[i]-n*glm::dot(n,T[i]));
        float w=(glm::dot(glm::cross(n,T[i]),B[i])<0.f)?-1.f:1.f;
        tans[i]=glm::vec4(t,w);
    }
}

// ── Public API ────────────────────────────────────────────────────────────────
bool Model::load(const std::string& path) {
    tinygltf::TinyGLTF loader;
    tinygltf::Model    gm;
    std::string        err, warn;

    bool ok;
    // Detect format: read first 4 bytes to check for GLB magic (0x46546C67 = "glTF")
    bool isBinary = false;
    {
        std::ifstream probe(path, std::ios::binary);
        if (probe) {
            uint32_t magic = 0;
            probe.read(reinterpret_cast<char*>(&magic), 4);
            isBinary = (magic == 0x46546C67u);
        }
    }
    if (isBinary)
        ok = loader.LoadBinaryFromFile(&gm, &err, &warn, path);
    else
        ok = loader.LoadASCIIFromFile(&gm, &err, &warn, path);

    if (!warn.empty()) std::cerr<<"[gltf] WARN: "<<warn<<"\n";
    if (!err.empty())  std::cerr<<"[gltf] ERR:  "<<err <<"\n";
    if (!ok) return false;

    if (gm.scenes.empty()) { std::cerr<<"[gltf] No scenes\n"; return false; }
    int si = gm.defaultScene >= 0 ? gm.defaultScene : 0;
    for (int n : gm.scenes[si].nodes)
        processNode(gm, n, glm::mat4(1));

    std::cout<<"[gltf] \""<<path<<"\" — "
             <<primitives.size()<<" prim(s), "<<nodes.size()<<" node(s)\n";
    return true;
}

void Model::destroy() {
    for (auto& p : primitives) {
        glDeleteVertexArrays(1,&p.vao);
        glDeleteBuffers(1,&p.vbo);
        if (p.ebo) glDeleteBuffers(1,&p.ebo);
    }
    for (auto& [_,t] : m_texCache) glDeleteTextures(1,&t);
    primitives.clear(); nodes.clear(); m_texCache.clear();
    aabbMin=glm::vec3(1e30f); aabbMax=glm::vec3(-1e30f);
}

// ── Draw ──────────────────────────────────────────────────────────────────────
void Model::draw(Shader& sh) const {
    for (const auto& n : nodes) drawNode(n,sh,glm::mat4(1));
}

void Model::drawNode(const Node& node, Shader& sh, const glm::mat4& parent) const {
    glm::mat4 world = parent * node.transform;
    sh.setMat4("uModel", world);

    for (int pi : node.primitiveIndices) {
        const Primitive& p = primitives[pi];

        // Alpha / culling
        if (p.doubleSided) glDisable(GL_CULL_FACE); else glEnable(GL_CULL_FACE);
        if (p.alphaMode == 2) { glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA); }
        else glDisable(GL_BLEND);

        // Material uniforms
        sh.setVec3 ("uBaseColor",        glm::vec3(p.baseColorFactor));
        sh.setFloat("uBaseAlpha",        p.baseColorFactor.a);
        sh.setVec3 ("uEmissive",         p.emissiveFactor);
        sh.setFloat("uMetallic",         p.metallicFactor);
        sh.setFloat("uRoughness",        p.roughnessFactor);
        sh.setFloat("uOcclusionStrength",p.occlusionStrength);
        sh.setFloat("uNormalScale",      p.normalScale);
        sh.setFloat("uAlphaCutoff",      p.alphaCutoff);
        sh.setInt  ("uAlphaMode",        p.alphaMode);
        sh.setBool ("uHasVertexColors",  p.hasVertexColors);
        sh.setBool ("uHasTangents",      p.hasTangents);

        // Textures
        auto bindTex = [&](GLuint tex, int slot, const char* hasBool, const char* samplerName){
            sh.setBool(hasBool, tex!=0);
            if (tex) {
                glActiveTexture(GL_TEXTURE0+slot);
                glBindTexture(GL_TEXTURE_2D, tex);
                sh.setInt(samplerName, slot);
            }
        };
        bindTex(p.baseColorTex,     0, "uHasBaseColorTex",     "uBaseColorTex");
        bindTex(p.normalTex,        1, "uHasNormalTex",        "uNormalTex");
        bindTex(p.metallicRoughTex, 2, "uHasMetallicRoughTex", "uMetallicRoughTex");
        bindTex(p.emissiveTex,      3, "uHasEmissiveTex",      "uEmissiveTex");
        bindTex(p.occlusionTex,     4, "uHasOcclusionTex",     "uOcclusionTex");

        glBindVertexArray(p.vao);
        if (p.hasIndices)
            glDrawElements(p.drawMode, p.indexCount, p.indexType, nullptr);
        else
            glDrawArrays(p.drawMode, 0, p.vertexCount);
        glBindVertexArray(0);
    }

    // Restore defaults
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);

    for (int c : node.children) drawNode(nodes[c], sh, world);
}

// ── Scene graph ───────────────────────────────────────────────────────────────
void Model::processNode(const tinygltf::Model& gm, int ni, const glm::mat4& parentT) {
    const auto& gn = gm.nodes[ni];
    Node node;
    node.transform = parentT * nodeLocalTransform(gn);

    if (gn.mesh >= 0) {
        int start = (int)primitives.size();
        processMesh(gm, gm.meshes[gn.mesh]);
        for (int i=start;i<(int)primitives.size();i++) node.primitiveIndices.push_back(i);
    }

    int myIdx = (int)nodes.size();
    nodes.push_back(node);

    for (int child : gn.children) {
        nodes[myIdx].children.push_back((int)nodes.size());
        processNode(gm, child, node.transform);
    }
}

void Model::processMesh(const tinygltf::Model& gm, const tinygltf::Mesh& mesh) {
    for (const auto& p : mesh.primitives)
        buildPrimitive(gm, p);
}

// ── Primitive builder ─────────────────────────────────────────────────────────
int Model::buildPrimitive(const tinygltf::Model& gm, const tinygltf::Primitive& prim) {
    Primitive out;

    // Draw mode
    switch(prim.mode){
        case TINYGLTF_MODE_POINTS:         out.drawMode=GL_POINTS;    break;
        case TINYGLTF_MODE_LINE:           out.drawMode=GL_LINES;     break;
        case TINYGLTF_MODE_LINE_STRIP:     out.drawMode=GL_LINE_STRIP;break;
        case TINYGLTF_MODE_LINE_LOOP:      out.drawMode=GL_LINE_LOOP; break;
        case TINYGLTF_MODE_TRIANGLE_STRIP: out.drawMode=GL_TRIANGLE_STRIP;break;
        case TINYGLTF_MODE_TRIANGLE_FAN:   out.drawMode=GL_TRIANGLE_FAN;  break;
        default:                           out.drawMode=GL_TRIANGLES; break;
    }

    auto findAttr=[&](const std::string& n)->const tinygltf::Accessor*{
        auto it=prim.attributes.find(n);
        return it==prim.attributes.end()?nullptr:&gm.accessors[it->second];
    };

    const auto* posAcc   = findAttr("POSITION");
    const auto* normAcc  = findAttr("NORMAL");
    const auto* tanAcc   = findAttr("TANGENT");
    const auto* uv0Acc   = findAttr("TEXCOORD_0");
    const auto* uv1Acc   = findAttr("TEXCOORD_1");
    const auto* colAcc   = findAttr("COLOR_0");

    if (!posAcc) { std::cerr<<"[gltf] No POSITION, skipping\n"; return -1; }
    size_t N = posAcc->count;

    // ── Read positions ────────────────────────────────────────────────────────
    std::vector<glm::vec3> positions(N);
    {
        const unsigned char* p = dataPtr(gm,*posAcc);
        size_t s = byteStride(gm,*posAcc,12);
        for (size_t i=0;i<N;i++){
            positions[i]=readVec3(p,s,i);
            aabbMin=glm::min(aabbMin,positions[i]);
            aabbMax=glm::max(aabbMax,positions[i]);
        }
    }

    // ── Read indices early (needed for normal/tangent generation) ─────────────
    std::vector<uint32_t> indices;
    if (prim.indices >= 0) {
        const auto& acc = gm.accessors[prim.indices];
        const auto& bv  = gm.bufferViews[acc.bufferView];
        const unsigned char* ip = gm.buffers[bv.buffer].data.data()+bv.byteOffset+acc.byteOffset;
        indices.resize(acc.count);
        if      (acc.componentType==TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
            for(size_t i=0;i<acc.count;i++) indices[i]=ip[i];
        else if (acc.componentType==TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
            for(size_t i=0;i<acc.count;i++) indices[i]=reinterpret_cast<const uint16_t*>(ip)[i];
        else
            memcpy(indices.data(),ip,acc.count*4);
    }

    // ── Normals ───────────────────────────────────────────────────────────────
    std::vector<glm::vec3> normals(N);
    if (normAcc) {
        const unsigned char* p=dataPtr(gm,*normAcc);
        size_t s=byteStride(gm,*normAcc,12);
        for(size_t i=0;i<N;i++) normals[i]=readVec3(p,s,i);
    } else {
        generateFlatNormals(normals,positions,indices);
    }

    // ── UVs ───────────────────────────────────────────────────────────────────
    std::vector<glm::vec2> uvs0(N,glm::vec2(0)), uvs1(N,glm::vec2(0));
    if (uv0Acc) {
        const unsigned char* p=dataPtr(gm,*uv0Acc);
        size_t s=byteStride(gm,*uv0Acc,8);
        for(size_t i=0;i<N;i++) uvs0[i]=readVec2(p,s,i,uv0Acc->componentType);
    }
    if (uv1Acc) {
        const unsigned char* p=dataPtr(gm,*uv1Acc);
        size_t s=byteStride(gm,*uv1Acc,8);
        for(size_t i=0;i<N;i++) uvs1[i]=readVec2(p,s,i,uv1Acc->componentType);
    }

    // ── Vertex colours ────────────────────────────────────────────────────────
    std::vector<glm::vec4> colors(N,glm::vec4(1));
    if (colAcc) {
        out.hasVertexColors=true;
        const unsigned char* p=dataPtr(gm,*colAcc);
        size_t comps=(colAcc->type==TINYGLTF_TYPE_VEC3)?3:4;
        size_t elemBytes=comps*(colAcc->componentType==TINYGLTF_COMPONENT_TYPE_FLOAT?4:
                                colAcc->componentType==TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT?2:1);
        size_t s=byteStride(gm,*colAcc,elemBytes);
        for(size_t i=0;i<N;i++){
            glm::vec4 c=readVec4(p,s,i,colAcc->componentType);
            if(comps==3) c.w=1.f;
            colors[i]=c;
        }
    }

    // ── Tangents ──────────────────────────────────────────────────────────────
    std::vector<glm::vec4> tangents(N,glm::vec4(1,0,0,1));
    if (tanAcc) {
        out.hasTangents=true;
        const unsigned char* p=dataPtr(gm,*tanAcc);
        size_t s=byteStride(gm,*tanAcc,16);
        for(size_t i=0;i<N;i++) tangents[i]=readVec4(p,s,i,TINYGLTF_COMPONENT_TYPE_FLOAT);
    } else if (uv0Acc) {
        out.hasTangents=true;
        generateTangents(tangents,positions,normals,uvs0,indices);
    }

    // ── Interleaved VBO layout ────────────────────────────────────────────────
    // pos(3) + norm(3) + tan(4) + uv0(2) + uv1(2) + color(4)  = 18 floats
    struct Vertex {
        float pos[3]; float norm[3]; float tan[4];
        float uv0[2]; float uv1[2]; float color[4];
    };
    std::vector<Vertex> verts(N);
    for(size_t i=0;i<N;i++){
        auto& v=verts[i];
        v.pos[0]=positions[i].x; v.pos[1]=positions[i].y; v.pos[2]=positions[i].z;
        v.norm[0]=normals[i].x;  v.norm[1]=normals[i].y;  v.norm[2]=normals[i].z;
        v.tan[0]=tangents[i].x;  v.tan[1]=tangents[i].y;
        v.tan[2]=tangents[i].z;  v.tan[3]=tangents[i].w;
        v.uv0[0]=uvs0[i].x;      v.uv0[1]=uvs0[i].y;
        v.uv1[0]=uvs1[i].x;      v.uv1[1]=uvs1[i].y;
        v.color[0]=colors[i].r;  v.color[1]=colors[i].g;
        v.color[2]=colors[i].b;  v.color[3]=colors[i].a;
    }

    glGenVertexArrays(1,&out.vao); glGenBuffers(1,&out.vbo);
    glBindVertexArray(out.vao);
    glBindBuffer(GL_ARRAY_BUFFER,out.vbo);
    glBufferData(GL_ARRAY_BUFFER,(GLsizeiptr)(N*sizeof(Vertex)),verts.data(),GL_STATIC_DRAW);

#define ATTRIB(loc,count,field) \
    glEnableVertexAttribArray(loc); \
    glVertexAttribPointer(loc,count,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)offsetof(Vertex,field));

    ATTRIB(0,3,pos)   // aPos
    ATTRIB(1,3,norm)  // aNormal
    ATTRIB(2,4,tan)   // aTangent
    ATTRIB(3,2,uv0)   // aUV0
    ATTRIB(4,2,uv1)   // aUV1
    ATTRIB(5,4,color) // aColor
#undef ATTRIB

    out.vertexCount=(GLsizei)N;

    // ── Index buffer ──────────────────────────────────────────────────────────
    if (!indices.empty()) {
        glGenBuffers(1,&out.ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,out.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,(GLsizeiptr)(indices.size()*4),indices.data(),GL_STATIC_DRAW);
        out.indexCount=(GLsizei)indices.size();
        out.indexType=GL_UNSIGNED_INT;
        out.hasIndices=true;
    }

    glBindVertexArray(0);

    // ── Material ──────────────────────────────────────────────────────────────
    if (prim.material >= 0) {
        const auto& mat = gm.materials[prim.material];
        const auto& pbr = mat.pbrMetallicRoughness;

        auto cf=pbr.baseColorFactor;
        if(cf.size()==4) out.baseColorFactor={cf[0],cf[1],cf[2],cf[3]};

        out.metallicFactor  = (float)pbr.metallicFactor;
        out.roughnessFactor = (float)pbr.roughnessFactor;

        auto ef=mat.emissiveFactor;
        if(ef.size()==3) out.emissiveFactor={ef[0],ef[1],ef[2]};

        out.normalScale       = (float)mat.normalTexture.scale;
        out.occlusionStrength = (float)mat.occlusionTexture.strength;
        out.doubleSided       = mat.doubleSided;
        out.alphaCutoff       = (float)mat.alphaCutoff;

        if      (mat.alphaMode=="MASK")  out.alphaMode=1;
        else if (mat.alphaMode=="BLEND") out.alphaMode=2;
        else                              out.alphaMode=0;

        // Choose UV set from texture's texCoord field
        auto loadTex=[&](int idx)->GLuint{
            return idx>=0 ? loadTexture(gm,idx) : 0;
        };

        if (pbr.baseColorTexture.index>=0) {
            out.baseColorTex=loadTex(pbr.baseColorTexture.index);
            out.hasBaseColorTex=(out.baseColorTex!=0);
        }
        if (mat.normalTexture.index>=0) {
            out.normalTex=loadTex(mat.normalTexture.index);
            out.hasNormalTex=(out.normalTex!=0);
        }
        if (pbr.metallicRoughnessTexture.index>=0) {
            out.metallicRoughTex=loadTex(pbr.metallicRoughnessTexture.index);
            out.hasMetallicRoughTex=(out.metallicRoughTex!=0);
        }
        if (mat.emissiveTexture.index>=0) {
            out.emissiveTex=loadTex(mat.emissiveTexture.index);
            out.hasEmissiveTex=(out.emissiveTex!=0);
        }
        if (mat.occlusionTexture.index>=0) {
            out.occlusionTex=loadTex(mat.occlusionTexture.index);
            out.hasOcclusionTex=(out.occlusionTex!=0);
        }
    }

    primitives.push_back(out);
    return (int)primitives.size()-1;
}

// ── Texture upload ────────────────────────────────────────────────────────────
GLuint Model::loadTexture(const tinygltf::Model& gm, int texIdx) {
    auto it=m_texCache.find(texIdx);
    if(it!=m_texCache.end()) return it->second;

    const auto& tex=gm.textures[texIdx];
    if(tex.source<0) return 0;
    const auto& img=gm.images[tex.source];
    if(img.image.empty()) return 0;

    // Always upload as RGBA to avoid Apple Metal sampler type mismatch
    // (GL_RED textures bound to a float sampler cause "unloadable" warnings on macOS)
    int components = img.component;
    std::vector<unsigned char> rgbaData;
    const unsigned char* pixelData = img.image.data();

    if (components == 1) {
        // Expand R -> RGBA (R,R,R,1)
        rgbaData.resize(img.width * img.height * 4);
        for (int i = 0; i < img.width * img.height; i++) {
            rgbaData[i*4+0] = img.image[i];
            rgbaData[i*4+1] = img.image[i];
            rgbaData[i*4+2] = img.image[i];
            rgbaData[i*4+3] = 255;
        }
        pixelData = rgbaData.data();
    } else if (components == 3) {
        // Expand RGB -> RGBA
        rgbaData.resize(img.width * img.height * 4);
        for (int i = 0; i < img.width * img.height; i++) {
            rgbaData[i*4+0] = img.image[i*3+0];
            rgbaData[i*4+1] = img.image[i*3+1];
            rgbaData[i*4+2] = img.image[i*3+2];
            rgbaData[i*4+3] = 255;
        }
        pixelData = rgbaData.data();
    }
    GLenum fmt=GL_RGBA, ifmt=GL_RGBA8;

    // Sampler wrapping / filtering
    GLenum wrapS=GL_REPEAT, wrapT=GL_REPEAT;
    GLenum minF=GL_LINEAR_MIPMAP_LINEAR, magF=GL_LINEAR;
    if (tex.sampler>=0) {
        const auto& samp=gm.samplers[tex.sampler];
        auto glWrap=[](int v)->GLenum{
            if(v==TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE)   return GL_CLAMP_TO_EDGE;
            if(v==TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT) return GL_MIRRORED_REPEAT;
            return GL_REPEAT;
        };
        wrapS=glWrap(samp.wrapS); wrapT=glWrap(samp.wrapT);
        if(samp.minFilter>0) minF=(GLenum)samp.minFilter;
        if(samp.magFilter>0) magF=(GLenum)samp.magFilter;
    }

    GLuint id;
    glGenTextures(1,&id);
    glBindTexture(GL_TEXTURE_2D,id);
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    glTexImage2D(GL_TEXTURE_2D,0,(GLint)ifmt,img.width,img.height,0,fmt,GL_UNSIGNED_BYTE,pixelData);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,wrapS);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,wrapT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,minF);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,magF);
    glBindTexture(GL_TEXTURE_2D,0);

    m_texCache[texIdx]=id;
    return id;
}