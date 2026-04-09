#!/bin/bash

# build.sh - Build and run the Loudness Leveler project

# Exit immediately if a command exits with a non-zero status
set -e

# Generate icon if needed
if [ -f "icon.png" ] && [ ! -f "icon.icns" ]; then
    echo "Generating icon.icns from icon.png..."
    chmod +x scripts/generate_icns.sh
    ./scripts/generate_icns.sh icon.png icon.icns
fi

# Create build directory if it doesn't exist
mkdir -p build
cd build

# Run CMake with Release configuration and speed optimizations
echo "Configuring project for maximum speed (Release mode)..."
cmake -DCMAKE_BUILD_TYPE=Release ..

# Build the project
echo "Building project..."
make -j$(sysctl -n hw.logicalcpu)

# --- App Bundle Creation ---
APP_NAME="loudness-leveler"
APP_BUNDLE="$APP_NAME.app"
CONTENTS="$APP_BUNDLE/Contents"
MACOS="$CONTENTS/MacOS"
FRAMEWORKS="$CONTENTS/Frameworks"
RESOURCES="$CONTENTS/Resources"

echo "Creating .app bundle structure..."
mkdir -p "$MACOS"
mkdir -p "$FRAMEWORKS"
mkdir -p "$RESOURCES"

# Copy binary (from current build directory)
cp "$APP_NAME" "$MACOS/"

# Copy icon if present
if [ -f "../icon.icns" ]; then
    cp "../icon.icns" "$RESOURCES/icon.icns"
fi

# Extract version from version.h
VERSION=$(grep "APP_VERSION_STR" ../version.h | cut -d '"' -f 2)
echo "Extracted version: $VERSION"

# Create Info.plist
cat > "$CONTENTS/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>$APP_NAME</string>
    <key>CFBundleIconFile</key>
    <string>icon.icns</string>
    <key>CFBundleIdentifier</key>
    <string>org.seegert.loudness-leveler</string>
    <key>CFBundleName</key>
    <string>Loudness Leveler</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>$VERSION</string>
    <key>CFBundleVersion</key>
    <string>$VERSION</string>
    <key>LSMinimumSystemVersion</key>
    <string>11.0</string>
</dict>
</plist>
EOF

# --- Dependency Collection (Dylibs) ---
echo "Collecting and relinking dependencies..."

# Function to fix a single file's dependencies
fix_dependencies() {
    local target="$1"
    echo "Processing $target..."
    
    # Find dependencies that point to Homebrew
    otool -L "$target" | grep -o "/opt/homebrew[^ ]*" | while read -r dep; do
        [ -z "$dep" ] && continue
        
        local lib_name=$(basename "$dep")
        echo "  Found dependency: $lib_name"
        
        if [ ! -f "$FRAMEWORKS/$lib_name" ]; then
            echo "    Copying $lib_name to bundle..."
            cp "$dep" "$FRAMEWORKS/"
            chmod +w "$FRAMEWORKS/$lib_name"
            # Recursively fix the newly copied library
            fix_dependencies "$FRAMEWORKS/$lib_name"
        fi
        
        # Update the load path in the target file
        echo "    Relinking $lib_name in $(basename "$target")..."
        install_name_tool -change "$dep" "@executable_path/../Frameworks/$lib_name" "$target"
    done
}

# Fix binary dependencies
if [ -f "$MACOS/$APP_NAME" ]; then
    echo "Found binary at $MACOS/$APP_NAME, starting fixup..."
    fix_dependencies "$MACOS/$APP_NAME"
else
    echo "ERROR: Binary not found at $MACOS/$APP_NAME"
    exit 1
fi

# Set the identification name of the copied libraries
for lib in "$FRAMEWORKS"/*.dylib; do
    if [ -f "$lib" ]; then
        lib_name=$(basename "$lib")
        install_name_tool -id "@executable_path/../Frameworks/$lib_name" "$lib"
    fi
done

# --- Code Signing ---
echo "Cleaning metadata files and ad-hoc signing (required for Apple Silicon)..."
# Remove any AppleDouble metadata files (._*) which cause codesign to fail on non-native filesystems
find "$APP_BUNDLE" -name "._*" -delete

# Force re-signing of all binaries and libraries deep inside the bundle
codesign --force --deep -s - "$APP_BUNDLE"

echo "Build successful! Portable bundle created at: $APP_NAME.app"

# If build is successful, run the executable from the bundle
echo "Starting loudness-leveler from bundle..."
"$MACOS/$APP_NAME"
