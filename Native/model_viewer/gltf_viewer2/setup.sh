#!/bin/bash
set -e
cd "$(dirname "$0")"

echo "==> Installing dependencies..."
brew install glfw glm cmake assimp

if [ ! -f stb_image.h ]; then
    echo "==> Downloading stb_image.h..."
    curl -sL "https://raw.githubusercontent.com/nothings/stb/master/stb_image.h" -o stb_image.h
    echo "    stb_image.h ✓"
fi

echo "==> Building..."
rm -rf build && mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH="$(brew --prefix glfw);$(brew --prefix glm);$(brew --prefix assimp)" \
         -DCMAKE_BUILD_TYPE=Release
make -j"$(sysctl -n hw.logicalcpu)"

echo ""
echo "✅  Done!  Run: ./model_viewer"
