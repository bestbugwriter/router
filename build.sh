#!/bin/bash

set -e

echo "Building C++ Log Collection and Forwarding System..."

# Create build directory
mkdir -p build
cd build

# Configure with CMake
echo "Configuring with CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
echo "Building..."
make -j$(nproc)

echo "Build completed successfully!"
echo "Executables:"
echo "  - log-agent"
echo "  - log-forwarder"
echo ""
echo "To run:"
echo "  ./log-agent ../config/agent.properties"
echo "  ./log-forwarder ../config/forwarder.properties"
