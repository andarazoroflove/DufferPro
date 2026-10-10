#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "=== BUILDING DUFFER PRO FOR MACOS SONOMA (UNIVERSAL 2: APPLE SILICON & INTEL) ==="

ZIG_BIN="/opt/zig/zig"
if [ ! -x "$ZIG_BIN" ]; then
    echo "Error: Zig compiler not found at $ZIG_BIN"
    exit 1
fi

LIPO_BIN="/usr/bin/llvm-lipo"
if [ ! -x "$LIPO_BIN" ]; then
    LIPO_BIN="$(which llvm-lipo-19 || which llvm-lipo || true)"
fi
if [ ! -x "$LIPO_BIN" ]; then
    echo "Error: llvm-lipo not found"
    exit 1
fi

# Ensure SDL2.framework is extracted
SDL_CACHE_DIR="/root/sdl_mac"
SDL_FRAMEWORK="$SDL_CACHE_DIR/SDL2/SDL2.framework"
if [ ! -d "$SDL_FRAMEWORK" ]; then
    echo "[1/7] Downloading and caching SDL2 macOS framework..."
    mkdir -p "$SDL_CACHE_DIR"
    curl -sSL -o "$SDL_CACHE_DIR/SDL2.dmg" "https://github.com/libsdl-org/SDL/releases/download/release-2.30.10/SDL2-2.30.10.dmg"
    cd "$SDL_CACHE_DIR" && 7z x SDL2.dmg >/dev/null
fi

mkdir -p "$PROJECT_ROOT/build/macos/obj"
OBJ_DIR="$PROJECT_ROOT/build/macos/obj"
INCLUDE_DIR="$PROJECT_ROOT/build/macos/include"
mkdir -p "$INCLUDE_DIR"
ln -sf "$SDL_FRAMEWORK/Headers" "$INCLUDE_DIR/SDL2"

RPATH_FLAGS="-Wl,-rpath,@executable_path/../Frameworks -Wl,-rpath,@executable_path/Frameworks -Wl,-rpath,@executable_path -Wl,-rpath,/Library/Frameworks"

echo "[2/7] Compiling Apple Silicon (arm64) slice..."
$ZIG_BIN cc -target aarch64-macos -O2 -I "$PROJECT_ROOT/src" -I "$INCLUDE_DIR" -I "$INCLUDE_DIR/SDL2" -c "$PROJECT_ROOT/src/golf_core.c" -o "$OBJ_DIR/golf_core_arm64.o"
$ZIG_BIN cc -target aarch64-macos -O2 -I "$PROJECT_ROOT/src" -I "$INCLUDE_DIR" -I "$INCLUDE_DIR/SDL2" -c "$PROJECT_ROOT/src/golf_render.c" -o "$OBJ_DIR/golf_render_arm64.o"
$ZIG_BIN cc -target aarch64-macos -O2 -I "$PROJECT_ROOT/src" -I "$INCLUDE_DIR" -I "$INCLUDE_DIR/SDL2" -c "$PROJECT_ROOT/src/golf_audio.c" -o "$OBJ_DIR/golf_audio_arm64.o"
$ZIG_BIN cc -target aarch64-macos -O2 -I "$PROJECT_ROOT/src" -I "$INCLUDE_DIR" -I "$INCLUDE_DIR/SDL2" -c "$PROJECT_ROOT/src/main.c" -o "$OBJ_DIR/main_arm64.o"
$ZIG_BIN cc -target aarch64-macos "$OBJ_DIR/golf_core_arm64.o" "$OBJ_DIR/golf_render_arm64.o" "$OBJ_DIR/golf_audio_arm64.o" "$OBJ_DIR/main_arm64.o" "$SDL_FRAMEWORK/Versions/A/SDL2" $RPATH_FLAGS -o "$OBJ_DIR/duffer_pro_arm64"

echo "[3/7] Compiling Intel (x86_64) slice..."
$ZIG_BIN cc -target x86_64-macos -O2 -I "$PROJECT_ROOT/src" -I "$INCLUDE_DIR" -I "$INCLUDE_DIR/SDL2" -c "$PROJECT_ROOT/src/golf_core.c" -o "$OBJ_DIR/golf_core_x86.o"
$ZIG_BIN cc -target x86_64-macos -O2 -I "$PROJECT_ROOT/src" -I "$INCLUDE_DIR" -I "$INCLUDE_DIR/SDL2" -c "$PROJECT_ROOT/src/golf_render.c" -o "$OBJ_DIR/golf_render_x86.o"
$ZIG_BIN cc -target x86_64-macos -O2 -I "$PROJECT_ROOT/src" -I "$INCLUDE_DIR" -I "$INCLUDE_DIR/SDL2" -c "$PROJECT_ROOT/src/golf_audio.c" -o "$OBJ_DIR/golf_audio_x86.o"
$ZIG_BIN cc -target x86_64-macos -O2 -I "$PROJECT_ROOT/src" -I "$INCLUDE_DIR" -I "$INCLUDE_DIR/SDL2" -c "$PROJECT_ROOT/src/main.c" -o "$OBJ_DIR/main_x86.o"
$ZIG_BIN cc -target x86_64-macos "$OBJ_DIR/golf_core_x86.o" "$OBJ_DIR/golf_render_x86.o" "$OBJ_DIR/golf_audio_x86.o" "$OBJ_DIR/main_x86.o" "$SDL_FRAMEWORK/Versions/A/SDL2" $RPATH_FLAGS -o "$OBJ_DIR/duffer_pro_x86"

echo "[4/7] Generating Universal 2 Mach-O fat binary..."
$LIPO_BIN -create -output "$PROJECT_ROOT/build/macos/duffer_pro" "$OBJ_DIR/duffer_pro_x86" "$OBJ_DIR/duffer_pro_arm64"
chmod 755 "$PROJECT_ROOT/build/macos/duffer_pro"
cp "$PROJECT_ROOT/build/macos/duffer_pro" "$PROJECT_ROOT/build/duffer_pro_mac"

echo "[5/7] Building 'Duffer Pro.app' bundle..."
APP_DIR="$PROJECT_ROOT/build/macos/Duffer Pro.app"
rm -rf "$APP_DIR"
mkdir -p "$APP_DIR/Contents/MacOS"
mkdir -p "$APP_DIR/Contents/Frameworks"
mkdir -p "$APP_DIR/Contents/Resources"

cp "$PROJECT_ROOT/build/macos/duffer_pro" "$APP_DIR/Contents/MacOS/duffer_pro"
chmod 755 "$APP_DIR/Contents/MacOS/duffer_pro"
cp "$PROJECT_ROOT/tools/Info.plist" "$APP_DIR/Contents/Info.plist"
echo -n "APPL????" > "$APP_DIR/Contents/PkgInfo"
if [ -f "$PROJECT_ROOT/tools/AppIcon.icns" ]; then
    cp "$PROJECT_ROOT/tools/AppIcon.icns" "$APP_DIR/Contents/Resources/AppIcon.icns"
fi
cp -a "$SDL_FRAMEWORK" "$APP_DIR/Contents/Frameworks/"

echo "[6/7] Building standalone portable distribution..."
PORTABLE_DIR="$PROJECT_ROOT/build/macos/duffer_pro_mac_portable"
rm -rf "$PORTABLE_DIR"
mkdir -p "$PORTABLE_DIR/Frameworks"
cp "$PROJECT_ROOT/build/macos/duffer_pro" "$PORTABLE_DIR/duffer_pro"
chmod 755 "$PORTABLE_DIR/duffer_pro"
cp -a "$SDL_FRAMEWORK" "$PORTABLE_DIR/Frameworks/"
cat << 'EOF' > "$PORTABLE_DIR/run.sh"
#!/usr/bin/env bash
HERE="$(cd "$(dirname "$0")" && pwd)"
exec "$HERE/duffer_pro" "$@"
EOF
chmod 755 "$PORTABLE_DIR/run.sh"

echo "[7/7] Creating distribution zip archives..."
cd "$PROJECT_ROOT/build/macos"
rm -f "Duffer_Pro_macOS_Sonoma.zip" "Duffer_Pro_macOS_Portable.zip"
zip -q -r -y "Duffer_Pro_macOS_Sonoma.zip" "Duffer Pro.app"
zip -q -r -y "Duffer_Pro_macOS_Portable.zip" "duffer_pro_mac_portable"

echo "=== MACOS SONOMA BUILD COMPLETE ==="
echo "App Bundle:     $APP_DIR"
echo "Universal Bin:  $PROJECT_ROOT/build/duffer_pro_mac"
echo "Distribution:   $PROJECT_ROOT/build/macos/Duffer_Pro_macOS_Sonoma.zip"
echo "Portable Zip:   $PROJECT_ROOT/build/macos/Duffer_Pro_macOS_Portable.zip"

