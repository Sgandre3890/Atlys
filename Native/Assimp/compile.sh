rm -rf build && mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH="$(brew --prefix glfw);$(brew --prefix glm);$(brew --prefix assimp)" \
         -DCMAKE_BUILD_TYPE=Release
make -j"$(sysctl -n hw.logicalcpu)"