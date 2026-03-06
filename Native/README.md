### For Native

This is to rebuild the cmake folder:
```bash
rm -rf build gltf_loader
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build --parallel
./gltf_loader
```
To compile after build with cmake:
```bash
cmake --build build --parallel
./gltf_loader
```