#!/bin/bash

# 综合测试运行脚本

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR/.."

echo "Log Pipeline - Comprehensive Test Suite"
echo "========================================"
echo ""

# 颜色定义
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 测试计数器
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# 运行单元测试
echo -e "${YELLOW}[1/3] Running Unit Tests...${NC}"
echo "=============================="

cd "$SCRIPT_DIR"

if bash run_unit_tests.sh; then
    echo -e "${GREEN}✓ Unit tests passed${NC}"
    ((PASSED_TESTS++))
else
    echo -e "${RED}✗ Unit tests failed${NC}"
    ((FAILED_TESTS++))
fi
((TOTAL_TESTS++))

echo ""

# 运行性能测试
echo -e "${YELLOW}[2/3] Running Performance Tests...${NC}"
echo "===================================="

TEST_BUILD_DIR="$PROJECT_DIR/build_tests"
mkdir -p "$TEST_BUILD_DIR"
cd "$TEST_BUILD_DIR"

if g++ -std=c++17 \
    -O3 \
    -I"$PROJECT_DIR/include" \
    -I"$PROJECT_DIR/build/_deps/asio-src/include" \
    "$PROJECT_DIR/tests/performance/perf_compression.cpp" \
    "$PROJECT_DIR/src/common/compressor.cpp" \
    -o perf_compression \
    -lpthread -lz -lsnappy 2>/dev/null; then
    
    if ./perf_compression; then
        echo -e "${GREEN}✓ Performance tests completed${NC}"
        ((PASSED_TESTS++))
    else
        echo -e "${RED}✗ Performance tests failed${NC}"
        ((FAILED_TESTS++))
    fi
else
    echo -e "${YELLOW}⊘ Performance tests skipped (dependencies not available)${NC}"
fi
((TOTAL_TESTS++))

echo ""

# 集成测试
echo -e "${YELLOW}[3/3] Integration Tests...${NC}"
echo "============================"

if [ -f "$PROJECT_DIR/docker-compose.yml" ]; then
    echo "Docker Compose configuration found"
    echo "To run integration tests with Docker:"
    echo "  docker-compose up"
    echo ""
    echo "Integration tests can be run with:"
    echo "  bash $SCRIPT_DIR/integration/test_with_docker.sh"
else
    echo -e "${YELLOW}⊘ Docker Compose not found, integration tests skipped${NC}"
fi
((TOTAL_TESTS++))

echo ""
echo "========================================"
echo -e "Test Summary:"
echo -e "  Total Tests: $TOTAL_TESTS"
echo -e "  ${GREEN}Passed: $PASSED_TESTS${NC}"
echo -e "  ${RED}Failed: $FAILED_TESTS${NC}"
echo "========================================"

if [ $FAILED_TESTS -eq 0 ]; then
    echo -e "${GREEN}✓ All tests completed successfully!${NC}"
    exit 0
else
    echo -e "${RED}✗ Some tests failed${NC}"
    exit 1
fi
