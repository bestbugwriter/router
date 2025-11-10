#!/bin/bash

# 单元测试运行脚本

set -e

echo "Building unit tests..."

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR/.."
BUILD_DIR="$PROJECT_DIR/build"
TEST_BUILD_DIR="$PROJECT_DIR/build_tests"

# 创建测试构建目录
mkdir -p "$TEST_BUILD_DIR"
cd "$TEST_BUILD_DIR"

# 编译单元测试
echo "Compiling test_protocol..."
g++ -std=c++17 \
    -I"$PROJECT_DIR/include" \
    -I"$BUILD_DIR/_deps/asio-src/include" \
    "$PROJECT_DIR/tests/unit/test_protocol.cpp" \
    "$PROJECT_DIR/src/common/protocol.cpp" \
    -o test_protocol \
    -lpthread

echo "Compiling test_compressor..."
g++ -std=c++17 \
    -I"$PROJECT_DIR/include" \
    -I"$BUILD_DIR/_deps/asio-src/include" \
    "$PROJECT_DIR/tests/unit/test_compressor.cpp" \
    "$PROJECT_DIR/src/common/compressor.cpp" \
    -o test_compressor \
    -lpthread -lz -lsnappy

# 运行单元测试
echo ""
echo "Running unit tests..."
echo "========================"

./test_protocol
./test_compressor

echo ""
echo "========================"
echo "✓ All unit tests passed!"
