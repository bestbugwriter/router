#!/bin/bash

set -e

echo "Building C++ Log Collection and Routing System..."

# 参数处理
BUILD_STATIC=0
BUILD_TYPE="Release"

while [[ $# -gt 0 ]]; do
    case $1 in
        --static)
            BUILD_STATIC=1
            shift
            ;;
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --help)
            echo "Usage: $0 [options]"
            echo "Options:"
            echo "  --static     Build static binary (default: dynamic)"
            echo "  --debug      Build debug binary (default: release)"
            echo "  --help       Show this help"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# 创建 build 目录
mkdir -p build
cd build

# 配置 CMake
echo "Configuring with CMake..."
CMAKE_ARGS="-DCMAKE_BUILD_TYPE=${BUILD_TYPE}"

if [ "$BUILD_STATIC" == "1" ]; then
    CMAKE_ARGS="${CMAKE_ARGS} -DBUILD_STATIC=ON"
    echo "Building static binaries"
else
    echo "Building dynamic binaries"
fi

cmake .. ${CMAKE_ARGS}

# 编译
echo "Building..."
make -j$(nproc)

echo ""
echo "Build completed successfully!"
echo ""
echo "Executables:"
echo "  - log-agent         (Log collector)"
echo "  - log-router        (Log router)"echo ""
echo "To run:"
echo "  ./log-agent ../config/agent.properties"
echo "  ./log-router ../config/router.properties"
echo ""
if [ "$BUILD_STATIC" == "1" ]; then
    echo "Note: Binaries are statically linked"
fi
