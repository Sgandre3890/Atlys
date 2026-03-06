### For Native

# This is to rebuild the cmake folder:
```bash
rm -rf build gltf_loader
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build --parallel
./gltf_loader
```
# To compile after build with cmake:
```bash
cmake --build build --parallel
./gltf_loader
```

# Potential year 2 concepts:
### Object-Oriented Programming

The Camera, Shader, and Model are all classes with clear separation of public interface and private implementation this is classic encapsulation. Camera has a private updateVectors() method that maintains internal state invariants, which is a textbook example of information hiding.
Shader uses static methods for compileShader and readFile methods that belong to the class but don't need an instance, which is a concept AP CS covers explicitly.

### Memory Management 

Model::destroy() manually calls glDeleteVertexArrays, glDeleteBuffers, and glDeleteTextures with manual resource cleanup, the C++ equivalent of what RAII and destructors formalize. This is a good example of why destructors exist.
The texture cache (m_texCache) uses an unordered_map to avoid uploading the same texture twice a practical application of hash maps for deduplication.

### Data Structures

std::vector<Primitive>, std::vector<Node>, std::vector<uint32_t> dynamic arrays used throughout for geometry data.
std::unordered_map<int, GLuint> for the texture cache O(1) average lookup vs O(n) linear search.
The scene graph (Node containing std::vector<int> children) is an adjacency list representation of a tree, which is a core data structures topic.

### Recursion

Model::processNode() and Model::drawNode() are both recursive and they call themselves on child nodes to traverse the scene tree. This is a clean real-world example of tree traversal via recursion.

### Pointers & References

Raw pointers used extensively in the accessor helpers (const unsigned char*) for reading binary buffer data with manual stride arithmetic with direct memory access.
g_cam is a raw pointer to a stack-allocated Camera, demonstrating pointer-to-local-variable lifetime concerns.
Lambda captures by reference in buildPrimitive and main are a modern C++ form of passing by reference.

### Enums & Type Safety

enum class CameraMovement and enum class AppState with scoped enums that prevent accidental integer comparisons, a step up from plain #define constants.

### Exception Handling

Shader's constructor throws std::runtime_error on compile/link failure, and main catches it with try/catch — the full exception propagation pattern.

### Algorithms & Math

The generateFlatNormals and generateTangents functions are implementations of geometric algorithms — cross products, dot products, normalization, and basis vector construction. These directly use linear algebra concepts.
The PBR shader implements the Cook-Torrance BRDF — GGX distribution, Schlick approximation, and Smith geometry functions. That's applied calculus and physics in code.
The camera's updateVectors() converts spherical coordinates (yaw/pitch) to Cartesian using cos/sin this is a direct application of trig.

### Abstraction Layers

The whole project is a good example of layered abstraction: GLFW handles the OS window → glad loads OpenGL function pointers → Shader wraps GLSL compilation → Model wraps geometry upload → main ties it together. Each layer only knows about the one below it.

### File I/O

Binary file reading with std::ifstream in binary mode, reading raw bytes and casting them to typed pointers — lower-level I/O than typical AP CS but the concepts (open, read, close, error check) are identical.
