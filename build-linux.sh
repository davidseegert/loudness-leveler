#!/bin/bash
# build-linux.sh - Build and package Loudness Leveler for Linux

# Exit immediately if a command exits with a non-zero status
set -e

# 1. Generate icon header from icon.png
if [ -f "./scripts/png_to_header.sh" ]; then
    echo "Generating icon header..."
    ./scripts/png_to_header.sh
else
    echo "Warning: scripts/png_to_header.sh not found."
fi

# 2. Create build directory and compile
echo "Configuring project (Release mode)..."
mkdir -p build
cd build

# Check if cmake is available
if ! command -v cmake &> /dev/null; then
    echo "Error: cmake is not installed. Please install it with: sudo apt install cmake"
    exit 1
fi

cmake -DCMAKE_BUILD_TYPE=Release ..

echo "Building project..."
make -j$(nproc)
cd ..

# 3. Prepare distribution folder
echo "Preparing distribution package..."
APP_NAME="loudness-leveler"
DIST_DIR="${APP_NAME}-linux"
rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR/libs"

# 4. Copy executable
if [ -f "build/$APP_NAME" ]; then
    cp "build/$APP_NAME" "$DIST_DIR/"
else
    echo "Error: Executable build/$APP_NAME not found."
    exit 1
fi

# 5. Gather dynamic library dependencies
echo "Gathering library dependencies..."
# Common system libraries that should NOT be bundled as they are part of the base OS
# and bundling them can cause compatibility issues (especially glibc).
EXCLUDE_PATTERN="linux-vdso.so|libpthread.so|libdl.so|libm.so|libc.so|ld-linux|librt.so|libgcc_s.so|libstdc++.so|libX11.so|libxcb.so|libXau.so|libXdmcp.so"

# Extract paths of libraries, filter them, and copy to libs folder
# We use -L to follow symlinks so we get the actual files
ldd "$DIST_DIR/$APP_NAME" | grep "=> /" | grep -vE "$EXCLUDE_PATTERN" | awk '{print $3}' | while read -r lib_path; do
    if [ -f "$lib_path" ]; then
        echo "Copying $lib_path"
        cp -L "$lib_path" "$DIST_DIR/libs/"
    fi
done

# 6. Create a launcher script to set LD_LIBRARY_PATH
echo "Creating launcher script..."
cat <<EOF > "$DIST_DIR/${APP_NAME}.sh"
#!/bin/bash
# Get the directory where this script is located
HERE="\$(dirname "\$(readlink -f "\$0")")"
# Set LD_LIBRARY_PATH to include our bundled libs
export LD_LIBRARY_PATH="\$HERE/libs:\$LD_LIBRARY_PATH"
# Run the actual executable
exec "\$HERE/${APP_NAME}" "\$@"
EOF
chmod +x "$DIST_DIR/${APP_NAME}.sh"

# 7. Create a README in the package
cat <<EOF > "$DIST_DIR/README.txt"
Loudness Leveler for Linux
=========================

To run the application, execute the '${APP_NAME}.sh' script:
    ./${APP_NAME}.sh

Requirements:
- GTK 3 and related system libraries should be installed on your system.
EOF

# 8. Zip the package
echo "Creating zip archive..."
zip -r "build/${DIST_DIR}.zip" "$DIST_DIR"

echo "--------------------------------------------------"
echo "Build and packaging complete!"
echo "Package: build/${DIST_DIR}.zip"
echo "To run: Extract and run ./${APP_NAME}.sh"
echo "--------------------------------------------------"
