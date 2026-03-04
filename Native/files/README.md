# GLTF / GLB Model Loader (OpenGL 3.3 + CMake)

A minimal but complete GLTF/GLB viewer built with:

| Library | Role |
|---------|------|
| **GLFW 3** | Window & input |
| **glad** | OpenGL function loader |
| **glm** | Math (vectors, matrices, quaternions) |
| **tinygltf** | GLTF 2.0 / GLB parsing + stb_image |

All dependencies are fetched automatically by CMake via `FetchContent` — no manual installs required.

---

## Prerequisites

| Tool | Minimum version |
|------|----------------|
| CMake | 3.16 |
| C++ compiler | C++17 (GCC 8+, Clang 7+, MSVC 2017+) |
| Git | Any recent version |

On **Linux** you also need the X11/Wayland dev libs for GLFW:

```bash
# Ubuntu / Debian
sudo apt install libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libxext-dev

# Fedora / RHEL
sudo dnf install libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel
```

---

## Build

```bash
# 1. Clone / unzip the project
cd gltf_loader

# 2. Configure (downloads deps on first run ~30 s)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# 3. Build
cmake --build build --parallel

# 4. Run
./build/gltf_loader path/to/model.glb
./build/gltf_loader path/to/model.gltf
```

> **Windows (MSVC):** open the generated `.sln`, or run
> `cmake --build build --config Release`

---

## Controls

| Input | Action |
|-------|--------|
| **W / A / S / D** | Move forward / left / back / right |
| **Q / E** | Move down / up |
| **Right-click + drag** | Look around |
| **Scroll wheel** | Zoom (FOV) |
| **ESC** | Quit |

The camera auto-fits to the model's bounding box on load.

---

## Project Layout

```
gltf_loader/
├── CMakeLists.txt
├── include/
│   ├── camera.h
│   ├── model.h
│   └── shader.h
├── src/
│   ├── main.cpp      ← window, render loop, camera input
│   ├── camera.cpp    ← fly-cam implementation
│   ├── model.cpp     ← tinygltf → VAO/VBO/EBO upload, draw
│   └── shader.cpp    ← GLSL compile / link helpers
└── shaders/
    ├── model.vert    ← world-space transform + normal matrix
    └── model.frag    ← Blinn-Phong + optional base-colour texture
```

---

## What's Supported

- ✅ Binary GLB and ASCII GLTF files
- ✅ Scene graph with node transforms (TRS & matrix)
- ✅ Multiple meshes / primitives per file
- ✅ POSITION, NORMAL, TEXCOORD_0 vertex attributes
- ✅ Indexed (uint8 / uint16 / uint32) and non-indexed geometry
- ✅ PBR `baseColorFactor` and `baseColorTexture`
- ✅ Texture caching (each image uploaded once)
- ✅ Auto bounding-box camera fit
- ✅ 4× MSAA

## Limitations / Possible Extensions

- No animations (skinning/morph targets)
- No PBR metallic-roughness lighting (only Blinn-Phong)
- No multiple UV sets beyond TEXCOORD_0
- No normal/occlusion/emissive maps (easy to add in the fragment shader)
