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

============================================================
 Model Viewer - Windows Setup (MSYS2/MinGW, no VS needed)
============================================================

[OK] MSYS2 found

[INFO] Updating package database...
:: Synchronizing package databases...
 clangarm64 is up to date
 mingw32 is up to date
 mingw64 is up to date
 ucrt64 is up to date
 clang64 is up to date
 msys is up to date

[INFO] Installing packages (may take a few minutes the first time)...

warning: git-2.53.0-1 is up to date -- skipping
warning: mingw-w64-x86_64-gcc-15.2.0-13 is up to date -- skipping
warning: mingw-w64-x86_64-cmake-4.2.3-1 is up to date -- skipping
warning: mingw-w64-x86_64-ninja-1.13.2-1 is up to date -- skipping
warning: mingw-w64-x86_64-glfw-3.4-1 is up to date -- skipping
warning: mingw-w64-x86_64-glm-1.0.3-1 is up to date -- skipping
warning: mingw-w64-x86_64-assimp-6.0.4-1 is up to date -- skipping
 there is nothing to do

[OK] Packages installed

[INFO] Building...

-- The CXX compiler identification is GNU 15.2.0
-- The C compiler identification is GNU 15.2.0
-- Detecting CXX compiler ABI info
-- Detecting CXX compiler ABI info - done
-- Check for working CXX compiler: C:/msys64/mingw64/bin/c++.exe - skipped
-- Detecting CXX compile features
-- Detecting CXX compile features - done
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working C compiler: C:/msys64/mingw64/bin/cc.exe - skipped
-- Detecting C compile features
-- Detecting C compile features - done
-- Found OpenGL: opengl32
-- Performing Test CMAKE_HAVE_LIBC_PTHREAD
-- Performing Test CMAKE_HAVE_LIBC_PTHREAD - Success
-- Found Threads: TRUE
-- Glad Library 'glad_gl_core'
CMake Error at C:/msys64/mingw64/share/cmake/Modules/FindPackageHandleStandardArgs.cmake:290 (message):
  Could NOT find Python (missing: Python_EXECUTABLE Interpreter)
Call Stack (most recent call first):
  C:/msys64/mingw64/share/cmake/Modules/FindPackageHandleStandardArgs.cmake:654 (_FPHSA_FAILURE_MESSAGE)
  C:/msys64/mingw64/share/cmake/Modules/FindPython.cmake:742 (find_package_handle_standard_args)
  build/_deps/glad-src/cmake/GladConfig.cmake:181 (find_package)
  CMakeLists.txt:20 (glad_add_library)


-- Configuring incomplete, errors occurred!

[ERROR] Build failed. See output above.
  Try opening the MSYS2 MINGW64 terminal manually and running:
    cd /your/project/path
    mkdir build && cd build
    cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/mingw64
    ninja
Press any key to continue . . .
