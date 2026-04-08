#!/bin/bash

# build-linux-arm.sh - Build and run the Loudness Leveler project on Linux ARM

# Exit immediately if a command exits with a non-zero status
set -e

# Create build directory if it doesn't exist
mkdir -p build
cd build

# Run CMake with Release configuration and speed optimizations
echo "Configuring project for maximum speed (Release mode)..."
cmake -DCMAKE_BUILD_TYPE=Release ..

# Build the project
echo "Building project..."
make -j$(nproc)

# If build is successful, run the executable
echo "Build successful! The executable is located at: build/loudness-leveler"
# Not running it automatically here as it might require a display (GUI)
