#!/bin/bash

# build.sh - Build and run the Loudness Leveler project

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
make -j$(sysctl -n hw.logicalcpu)

# If build is successful, run the executable
echo "Build successful! Starting loudness-leveler..."
./loudness-leveler
