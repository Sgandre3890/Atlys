#!/bin/bash
# ─────────────────────────────────────────────────────────────
#  gltf_loader – local Homebrew install
#  Usage: bash install.sh
# ─────────────────────────────────────────────────────────────
set -euo pipefail

FORMULA_DIR="$(cd "$(dirname "$0")" && pwd)"
BINARY="$FORMULA_DIR/gltf_loader"
SHARE_DIR="$(brew --prefix)/share/gltf_loader"
BIN_DIR="$(brew --prefix)/bin"

echo ""
echo "  GLTF Loader – installer"
echo "──────────────────────────────────"

# 1. Check / install Homebrew
if ! command -v brew &>/dev/null; then
  echo "▸ Installing Homebrew..."
  /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
else
  echo "✓ Homebrew found: $(brew --version | head -1)"
fi

# 2. Install dependencies
echo "▸ Installing dependencies (glfw, glm, cmake)..."
brew install glfw glm cmake 2>/dev/null || true
echo "✓ Dependencies ready"

# 3. Build
echo "▸ Building..."
cd "$FORMULA_DIR"
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DCMAKE_RUNTIME_OUTPUT_DIRECTORY="$FORMULA_DIR" \
  --log-level=ERROR
cmake --build build --parallel --target gltf_loader 2>&1 | tail -3

if [ ! -f "$BINARY" ]; then
  echo "✗ Build failed – binary not found at $BINARY"
  exit 1
fi
echo "✓ Build complete"

# 4. Install binary
echo "▸ Installing binary to $BIN_DIR..."
cp "$BINARY" "$BIN_DIR/gltf_loader"
chmod 755 "$BIN_DIR/gltf_loader"

# 5. Install shaders
echo "▸ Installing shaders to $SHARE_DIR..."
mkdir -p "$SHARE_DIR/shaders"
cp "$FORMULA_DIR/shaders/"* "$SHARE_DIR/shaders/"

echo ""
echo "──────────────────────────────────"
echo "✓ Done! Run it with:"
echo ""
echo "    gltf_loader                  # open file picker"
echo "    gltf_loader path/to/model.glb"
echo ""