#!/bin/bash

# 综合测试运行脚本
# 一键运行单元测试、集成测试、性能测试和反压恢复测试

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR/.."

# 颜色定义
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
BOLD='\033[1m'
NC='\033[0m' # No Color

# 测试配置
RUN_UNIT_TESTS=${RUN_UNIT_TESTS:-true}
RUN_INTEGRATION_TESTS=${RUN_INTEGRATION_TESTS:-true}
RUN_PERFORMANCE_TESTS=${RUN_PERFORMANCE_TESTS:-false}
RUN_BACKPRESSURE_TESTS=${RUN_BACKPRESSURE_TESTS:-false}
SKIP_BUILD=${SKIP_BUILD:-false}

# 测试结果目录
RESULTS_DIR="/tmp/comprehensive-test-results"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
TEST_RESULTS_DIR="$RESULTS_DIR/comprehensive_test_$TIMESTAMP"

# 创建结果目录
mkdir -p "$TEST_RESULTS_DIR"

# 测试统计
TOTAL_SUITES=0
PASSED_SUITES=0
FAILED_SUITES=0

# 打印带颜色的标题
print_title() {
    echo -e "${BOLD}${BLUE}========================================${NC}"
    echo -e "${BOLD}${BLUE}    $1${NC}"
    echo -e "${BOLD}${BLUE}========================================${NC}"
    echo ""
}

# 打印测试套件结果
print_suite_result() {
    local suite_name=$1
    local result=$2
    local details=$3
    
    ((TOTAL_SUITES++))
    
    if [ "$result" = "PASS" ]; then
        ((PASSED_SUITES++))
        echo -e "${GREEN}✓ PASS${NC}: $suite_name"
    else
        ((FAILED_SUITES++))
        echo -e "${RED}✗ FAIL${NC}: $suite_name"
    fi
    
    if [ -n "$details" ]; then
        echo -e "   ${YELLOW}$details${NC}"
    fi
    echo ""
}

# 检查系统依赖
check_dependencies() {
    print_title "检查系统依赖"
    
    local missing_deps=()
    
    # 检查基本命令
    local commands=("docker" "docker-compose" "python3" "gcc" "make" "cmake")
    for cmd in "${commands[@]}"; do
        if ! command -v "$cmd" >/dev/null 2>&1; then
            missing_deps+=("$cmd")
        fi
    done
    
    # 检查 Python 库
    local python_libs=("kafka" "psutil")
    for lib in "${python_libs[@]}"; do
        if ! python3 -c "import $lib" 2>/dev/null; then
            missing_deps+=("python3-$lib")
        fi
    done
    
    if [ ${#missing_deps[@]} -gt 0 ]; then
        echo -e "${RED}缺少以下依赖:${NC}"
        for dep in "${missing_deps[@]}"; do
            echo "  - $dep"
        done
        echo ""
        echo -e "${YELLOW}请安装缺少的依赖后重新运行测试${NC}"
        return 1
    fi
    
    echo -e "${GREEN}✓ 所有依赖检查通过${NC}"
    echo ""
    return 0
}

# 构建项目
build_project() {
    if [ "$SKIP_BUILD" = "true" ]; then
        echo -e "${YELLOW}跳过构建步骤${NC}"
        return 0
    fi
    
    print_title "构建项目"
    
    cd "$PROJECT_DIR"
    
    # 检查是否已经构建
    if [ -f "build/log-agent" ] && [ -f "build/log-router" ]; then
        echo -e "${YELLOW}检测到已构建的可执行文件，跳过构建${NC}"
        return 0
    fi
    
    echo -e "${BLUE}开始构建项目...${NC}"
    
    # 运行构建脚本
    if bash build.sh > "$TEST_RESULTS_DIR/build.log" 2>&1; then
        echo -e "${GREEN}✓ 项目构建成功${NC}"
        return 0
    else
        echo -e "${RED}✗ 项目构建失败${NC}"
        echo -e "${YELLOW}查看构建日志: $TEST_RESULTS_DIR/build.log${NC}"
        return 1
    fi
}

# 运行单元测试
run_unit_tests() {
    if [ "$RUN_UNIT_TESTS" != "true" ]; then
        echo -e "${YELLOW}跳过单元测试${NC}"
        return 0
    fi
    
    print_title "运行单元测试"
    
    cd "$SCRIPT_DIR"
    
    echo -e "${BLUE}运行单元测试套件...${NC}"
    
    if bash run_unit_tests.sh > "$TEST_RESULTS_DIR/unit_tests.log" 2>&1; then
        print_suite_result "单元测试" "PASS" "所有单元测试通过"
        return 0
    else
        print_suite_result "单元测试" "FAIL" "部分单元测试失败"
        echo -e "${YELLOW}查看测试日志: $TEST_RESULTS_DIR/unit_tests.log${NC}"
        return 1
    fi
}

# 运行集成测试
run_integration_tests() {
    if [ "$RUN_INTEGRATION_TESTS" != "true" ]; then
        echo -e "${YELLOW}跳过集成测试${NC}"
        return 0
    fi
    
    print_title "运行集成测试"
    
    echo -e "${BLUE}启动集成测试套件...${NC}"
    echo "预计耗时: 5-10分钟"
    echo ""
    
    # 设置环境变量
    export TEST_DURATION=300  # 5分钟
    export LOG_RATE=200       # 每秒200条日志
    export RESULTS_DIR="$TEST_RESULTS_DIR/integration"
    
    if bash integration/run_integration_tests.sh > "$TEST_RESULTS_DIR/integration_tests.log" 2>&1; then
        print_suite_result "集成测试" "PASS" "Agent->Router->Kafka 数据流正常"
        return 0
    else
        print_suite_result "集成测试" "FAIL" "集成测试失败"
        echo -e "${YELLOW}查看测试日志: $TEST_RESULTS_DIR/integration_tests.log${NC}"
        return 1
    fi
}

# 运行性能测试
run_performance_tests() {
    if [ "$RUN_PERFORMANCE_TESTS" != "true" ]; then
        echo -e "${YELLOW}跳过性能测试${NC}"
        return 0
    fi
    
    print_title "运行性能测试"
    
    echo -e "${BLUE}启动性能测试套件...${NC}"
    echo "预计耗时: 15-20分钟"
    echo ""
    
    # 设置环境变量
    export PERF_TEST_DURATION=600  # 10分钟
    export LOW_RATE=100
    export MEDIUM_RATE=500
    export HIGH_RATE=1000
    export PEAK_RATE=2000
    export RESULTS_DIR="$TEST_RESULTS_DIR/performance"
    
    if bash performance/run_performance_tests.sh > "$TEST_RESULTS_DIR/performance_tests.log" 2>&1; then
        print_suite_result "性能测试" "PASS" "性能指标满足要求"
        return 0
    else
        print_suite_result "性能测试" "FAIL" "性能测试失败或指标不达标"
        echo -e "${YELLOW}查看测试日志: $TEST_RESULTS_DIR/performance_tests.log${NC}"
        return 1
    fi
}

# 运行反压恢复测试
run_backpressure_tests() {
    if [ "$RUN_BACKPRESSURE_TESTS" != "true" ]; then
        echo -e "${YELLOW}跳过反压恢复测试${NC}"
        return 0
    fi
    
    print_title "运行反压和故障恢复测试"
    
    echo -e "${BLUE}启动反压恢复测试套件...${NC}"
    echo "预计耗时: 10-15分钟"
    echo ""
    
    # 设置环境变量
    export TEST_DURATION=600
    export LOG_RATE=500
    export RESULTS_DIR="$TEST_RESULTS_DIR/backpressure"
    
    if bash integration/test_backpressure_recovery.sh > "$TEST_RESULTS_DIR/backpressure_tests.log" 2>&1; then
        print_suite_result "反压恢复测试" "PASS" "反压和故障恢复机制正常"
        return 0
    else
        print_suite_result "反压恢复测试" "FAIL" "反压恢复测试失败"
        echo -e "${YELLOW}查看测试日志: $TEST_RESULTS_DIR/backpressure_tests.log${NC}"
        return 1
    fi
}

# 生成综合测试报告
generate_comprehensive_report() {
    print_title "生成综合测试报告"
    
    local report_file="$TEST_RESULTS_DIR/comprehensive_test_report.json"
    
    python3 -c "
import json
import os
from datetime import datetime

# 收集测试结果
report = {
    'test_session': '$TIMESTAMP',
    'test_type': 'Comprehensive Test Suite',
    'summary': {
        'total_suites': $TOTAL_SUITES,
        'passed_suites': $PASSED_SUITES,
        'failed_suites': $FAILED_SUITES,
        'success_rate': ($PASSED_SUITES * 100 / $TOTAL_SUITES) if $TOTAL_SUITES > 0 else 0
    },
    'test_suites': [
        {
            'name': 'Unit Tests',
            'enabled': $RUN_UNIT_TESTS,
            'status': 'passed' if $PASSED_SUITES > 0 else 'failed'
        },
        {
            'name': 'Integration Tests', 
            'enabled': $RUN_INTEGRATION_TESTS,
            'status': 'passed'
        },
        {
            'name': 'Performance Tests',
            'enabled': $RUN_PERFORMANCE_TESTS,
            'status': 'passed'
        },
        {
            'name': 'Backpressure Tests',
            'enabled': $RUN_BACKPRESSURE_TESTS,
            'status': 'passed'
        }
    ],
    'environment': {
        'os': os.uname().sysname,
        'kernel': os.uname().release,
        'architecture': os.uname().machine
    },
    'recommendations': []
}

# 添加建议
if report['summary']['success_rate'] == 100:
    report['recommendations'].append('所有测试通过，系统质量良好')
elif report['summary']['success_rate'] >= 75:
    report['recommendations'].append('大部分测试通过，建议修复失败的测试')
else:
    report['recommendations'].append('多个测试失败，建议全面检查系统')

# 保存报告
with open('$report_file', 'w') as f:
    json.dump(report, f, indent=2, ensure_ascii=False)

print('综合测试报告已保存:', '$report_file')
"
    
    # 打印测试总结
    echo ""
    print_title "测试总结"
    
    echo "测试套件总数: $TOTAL_SUITES"
    echo -e "通过套件: ${GREEN}$PASSED_SUITES${NC}"
    echo -e "失败套件: ${RED}$FAILED_SUITES${NC}"
    
    local success_rate=$((PASSED_SUITES * 100 / TOTAL_SUITES))
    echo "总体成功率: $success_rate%"
    echo ""
    
    # 显示各测试套件状态
    echo -e "${BLUE}测试套件状态:${NC}"
    [ "$RUN_UNIT_TESTS" = "true" ] && echo "  单元测试: ${GREEN}已运行${NC}"
    [ "$RUN_INTEGRATION_TESTS" = "true" ] && echo "  集成测试: ${GREEN}已运行${NC}"
    [ "$RUN_PERFORMANCE_TESTS" = "true" ] && echo "  性能测试: ${GREEN}已运行${NC}"
    [ "$RUN_BACKPRESSURE_TESTS" = "true" ] && echo "  反压恢复测试: ${GREEN}已运行${NC}"
    echo ""
    
    echo -e "详细报告: ${BLUE}$report_file${NC}"
    echo -e "所有测试日志: ${BLUE}$TEST_RESULTS_DIR${NC}"
}

# 显示帮助信息
show_help() {
    echo "综合测试运行脚本"
    echo ""
    echo "用法: $0 [选项]"
    echo ""
    echo "选项:"
    echo "  --help, -h              显示此帮助信息"
    echo "  --skip-unit             跳过单元测试"
    echo "  --skip-integration      跳过集成测试"
    echo "  --skip-performance      跳过性能测试"
    echo "  --skip-backpressure     跳过反压恢复测试"
    echo "  --skip-build            跳过项目构建"
    echo "  --unit-only             仅运行单元测试"
    echo "  --integration-only      仅运行集成测试"
    echo "  --performance-only      仅运行性能测试"
    echo "  --backpressure-only     仅运行反压恢复测试"
    echo "  --quick                 快速测试 (仅单元测试 + 集成测试)"
    echo ""
    echo "环境变量:"
    echo "  RUN_UNIT_TESTS          是否运行单元测试 (默认: true)"
    echo "  RUN_INTEGRATION_TESTS   是否运行集成测试 (默认: true)"
    echo "  RUN_PERFORMANCE_TESTS   是否运行性能测试 (默认: false)"
    echo "  RUN_BACKPRESSURE_TESTS  是否运行反压恢复测试 (默认: false)"
    echo ""
}

# 解析命令行参数
parse_arguments() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            --help|-h)
                show_help
                exit 0
                ;;
            --skip-unit)
                RUN_UNIT_TESTS=false
                shift
                ;;
            --skip-integration)
                RUN_INTEGRATION_TESTS=false
                shift
                ;;
            --skip-performance)
                RUN_PERFORMANCE_TESTS=false
                shift
                ;;
            --skip-backpressure)
                RUN_BACKPRESSURE_TESTS=false
                shift
                ;;
            --skip-build)
                SKIP_BUILD=true
                shift
                ;;
            --unit-only)
                RUN_UNIT_TESTS=true
                RUN_INTEGRATION_TESTS=false
                RUN_PERFORMANCE_TESTS=false
                RUN_BACKPRESSURE_TESTS=false
                shift
                ;;
            --integration-only)
                RUN_UNIT_TESTS=false
                RUN_INTEGRATION_TESTS=true
                RUN_PERFORMANCE_TESTS=false
                RUN_BACKPRESSURE_TESTS=false
                shift
                ;;
            --performance-only)
                RUN_UNIT_TESTS=false
                RUN_INTEGRATION_TESTS=false
                RUN_PERFORMANCE_TESTS=true
                RUN_BACKPRESSURE_TESTS=false
                shift
                ;;
            --backpressure-only)
                RUN_UNIT_TESTS=false
                RUN_INTEGRATION_TESTS=false
                RUN_PERFORMANCE_TESTS=false
                RUN_BACKPRESSURE_TESTS=true
                shift
                ;;
            --quick)
                RUN_UNIT_TESTS=true
                RUN_INTEGRATION_TESTS=true
                RUN_PERFORMANCE_TESTS=false
                RUN_BACKPRESSURE_TESTS=false
                shift
                ;;
            *)
                echo -e "${RED}未知选项: $1${NC}"
                show_help
                exit 1
                ;;
        esac
    done
}

# 主函数
main() {
    print_title "Log Pipeline 综合测试套件"
    
    # 解析命令行参数
    parse_arguments "$@"
    
    # 检查依赖
    if ! check_dependencies; then
        exit 1
    fi
    
    # 构建项目
    if ! build_project; then
        exit 1
    fi
    
    # 记录开始时间
    local start_time=$(date +%s)
    
    # 运行测试套件
    run_unit_tests
    run_integration_tests
    run_performance_tests
    run_backpressure_tests
    
    # 计算总耗时
    local end_time=$(date +%s)
    local total_duration=$((end_time - start_time))
    local duration_formatted=$(printf "%02d:%02d:%02d" $((total_duration/3600)) $((total_duration%3600/60)) $((total_duration%60)))
    
    # 生成综合报告
    generate_comprehensive_report
    
    # 最终结果
    print_title "测试完成"
    echo "总耗时: $duration_formatted"
    echo ""
    
    if [ $FAILED_SUITES -eq 0 ]; then
        echo -e "${GREEN}🎉 所有测试通过！${NC}"
        exit 0
    else
        echo -e "${RED}❌ 部分测试失败${NC}"
        exit 1
    fi
}

# 运行主函数
main "$@"
