### This is the native version

rm -rf build gltf_loader
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build --parallel
./gltf_loader

cmake --build build --parallel
./gltf_loader