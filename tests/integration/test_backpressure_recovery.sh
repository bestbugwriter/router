#!/bin/bash

# 反压和故障恢复测试脚本
# 测试 Kafka 限流反压、Agent/Router 宕机恢复等场景

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR/../.."
TOOLS_DIR="$PROJECT_DIR/tests/tools"

# 颜色定义
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 测试配置
TEST_DURATION=${TEST_DURATION:-600}     # 10分钟
LOG_RATE=${LOG_RATE:-500}               # 每秒500条日志
KAFKA_TOPIC=${KAFKA_TOPIC:-"backpressure-test"}
RESULTS_DIR="/tmp/backpressure-test-results"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
TEST_RESULTS_DIR="$RESULTS_DIR/backpressure_test_$TIMESTAMP"

# 创建结果目录
mkdir -p "$TEST_RESULTS_DIR"

# Docker Compose 文件
COMPOSE_FILE="$PROJECT_DIR/docker-compose.yml"

# 测试计数器
TESTS_PASSED=0
TESTS_TOTAL=0

# 测试结果记录
test_result() {
    local test_name=$1
    local result=$2
    local message=$3
    
    ((TESTS_TOTAL++))
    
    if [ "$result" = "PASS" ]; then
        ((TESTS_PASSED++))
        echo -e "${GREEN}✓ PASS${NC}: $test_name - $message"
    else
        echo -e "${RED}✗ FAIL${NC}: $test_name - $message"
    fi
    
    echo "$(date '+%Y-%m-%d %H:%M:%S'),$test_name,$result,$message" >> "$TEST_RESULTS_DIR/test_results.csv"
}

# 启动基础测试环境
start_test_environment() {
    echo -e "${BLUE}启动反压测试环境...${NC}"
    
    cd "$PROJECT_DIR"
    
    # 启动基础服务
    docker-compose -f "$COMPOSE_FILE" up -d zookeeper kafka control-platform monitor
    
    # 等待服务启动
    echo -e "${YELLOW}等待服务启动...${NC}"
    sleep 20
    
    # 检查 Kafka
    local attempts=0
    while [ $attempts -lt 30 ]; do
        if docker exec kafka kafka-topics --bootstrap-server localhost:9092 --list >/dev/null 2>&1; then
            break
        fi
        sleep 2
        ((attempts++))
    done
    
    if [ $attempts -eq 30 ]; then
        echo -e "${RED}✗ Kafka 启动失败${NC}"
        return 1
    fi
    
    # 创建测试 topic
    docker exec kafka kafka-topics --create --topic "$KAFKA_TOPIC" --bootstrap-server localhost:9092 --partitions 3 --replication-factor 1 --if-not-exists
    
    # 创建测试配置
    cat > "$PROJECT_DIR/config/test-router.properties" << EOF
# Router 反压测试配置
router.id=backpressure-test-router
router.name=Backpressure Test Router

# 监听配置
listen.host=0.0.0.0
listen.port=9090
listen.backlog=1024

# IO 线程配置
io.threads=8
session.buffer_size=65536
session.max_connections=1000

# Kafka 配置
kafka.clusters=default
kafka.default.brokers=kafka:29092
kafka.default.topic=$KAFKA_TOPIC
kafka.default.producer.acks=1
kafka.default.producer.retries=3
kafka.default.producer.batch_size=16384
kafka.default.producer.linger_ms=10

# 压缩配置
compression.enabled=true
compression.algorithm=snappy

# 指标配置
metrics.enabled=true
metrics.port=9101
metrics.interval=5000
EOF

    # 启动 Agent 和 Router
    docker-compose -f "$COMPOSE_FILE" up -d agent-1 router
    
    # 等待启动完成
    sleep 15
    
    echo -e "${GREEN}✓ 反压测试环境启动成功${NC}"
}

# 测试 1: Kafka 限流反压测试
test_kafka_throttling() {
    echo -e "${BLUE}测试 1: Kafka 限流反压${NC}"
    
    local test_name="kafka_throttling"
    local test_duration=120  # 2分钟
    
    # 获取初始 Kafka 消费者组延迟
    local initial_lag=0
    docker exec kafka kafka-consumer-groups --bootstrap-server localhost:9092 --describe --group log-router-group >/dev/null 2>&1 || true
    
    # 启动日志生成器
    python3 "$TOOLS_DIR/log_generator.py" \
        --output-dir "/tmp/backpressure-logs-$test_name" \
        --duration "$test_duration" \
        --rate "$LOG_RATE" \
        --format json \
        > "$TEST_RESULTS_DIR/${test_name}_log_generator.log" 2>&1 &
    
    local log_gen_pid=$!
    
    # 等待系统稳定
    sleep 30
    
    # 模拟 Kafka 限流 - 降低生产者配额
    echo -e "${YELLOW}模拟 Kafka 限流...${NC}"
    docker exec kafka kafka-configs --bootstrap-server localhost:9092 --alter --entity-type producers --entity-name default --add-config producer-byte-rate=102400 >/dev/null 2>&1 || true
    
    # 监控反压效果
    local backpressure_detected=false
    local monitoring_duration=60
    local start_time=$(date +%s)
    
    while [ $(($(date +%s) - start_time)) -lt $monitoring_duration ]; do
        # 检查 Agent 日志中是否有反压相关信息
        if docker logs agent-1 --tail 50 2>&1 | grep -i "backpressure\|buffer.*full\|slow.*consumer" >/dev/null; then
            backpressure_detected=true
            break
        fi
        
        # 检查 Router 日志
        if docker logs router --tail 50 2>&1 | grep -i "backpressure\|buffer.*full\|kafka.*slow" >/dev/null; then
            backpressure_detected=true
            break
        fi
        
        sleep 5
    done
    
    # 恢复 Kafka 配置
    docker exec kafka kafka-configs --bootstrap-server localhost:9092 --alter --entity-type producers --entity-name default --delete-config producer-byte-rate >/dev/null 2>&1 || true
    
    # 等待日志生成器完成
    wait $log_gen_pid 2>/dev/null || true
    
    # 评估测试结果
    if [ "$backpressure_detected" = true ]; then
        test_result "$test_name" "PASS" "成功检测到 Kafka 限流导致的反压"
    else
        test_result "$test_name" "FAIL" "未检测到预期的反压现象"
    fi
    
    # 收集日志
    docker logs agent-1 --tail 200 > "$TEST_RESULTS_DIR/${test_name}_agent.log" 2>&1
    docker logs router --tail 200 > "$TEST_RESULTS_DIR/${test_name}_router.log" 2>&1
}

# 测试 2: Router 宕机恢复测试
test_router_failure_recovery() {
    echo -e "${BLUE}测试 2: Router 宕机恢复${NC}"
    
    local test_name="router_failure_recovery"
    local test_duration=180  # 3分钟
    
    # 启动日志生成器
    python3 "$TOOLS_DIR/log_generator.py" \
        --output-dir "/tmp/backpressure-logs-$test_name" \
        --duration "$test_duration" \
        --rate "$((LOG_RATE / 2))" \
        --format json \
        > "$TEST_RESULTS_DIR/${test_name}_log_generator.log" 2>&1 &
    
    local log_gen_pid=$!
    
    # 等待系统稳定运行
    sleep 30
    
    # 记录宕机前的消息处理情况
    local messages_before=0
    if docker exec kafka kafka-run-class kafka.tools.GetOffsetShell --broker-list localhost:9092 --topic "$KAFKA_TOPIC" >/dev/null 2>&1; then
        messages_before=$(docker exec kafka kafka-run-class kafka.tools.GetOffsetShell --broker-list localhost:9092 --topic "$KAFKA_TOPIC" | awk -F: '{sum+=$3} END {print sum+0}')
    fi
    
    # 模拟 Router 宕机
    echo -e "${YELLOW}模拟 Router 宕机...${NC}"
    docker-compose -f "$COMPOSE_FILE" stop router
    
    # 等待一段时间确保 Agent 检测到连接断开
    sleep 20
    
    # 恢复 Router
    echo -e "${YELLOW}恢复 Router 服务...${NC}"
    docker-compose -f "$COMPOSE_FILE" start router
    
    # 等待 Router 重新启动
    sleep 30
    
    # 检查 Router 是否成功重启
    local router_restarted=false
    if docker ps | grep router >/dev/null; then
        # 检查 Router 是否监听端口
        if docker exec router netstat -tlnp | grep :9090 >/dev/null; then
            router_restarted=true
        fi
    fi
    
    # 等待日志生成器完成
    wait $log_gen_pid 2>/dev/null || true
    
    # 记录恢复后的消息处理情况
    local messages_after=0
    if docker exec kafka kafka-run-class kafka.tools.GetOffsetShell --broker-list localhost:9092 --topic "$KAFKA_TOPIC" >/dev/null 2>&1; then
        messages_after=$(docker exec kafka kafka-run-class kafka.tools.GetOffsetShell --broker-list localhost:9092 --topic "$KAFKA_TOPIC" | awk -F: '{sum+=$3} END {print sum+0}')
    fi
    
    # 评估测试结果
    local messages_processed=$((messages_after - messages_before))
    
    if [ "$router_restarted" = true ] && [ $messages_processed -gt 0 ]; then
        test_result "$test_name" "PASS" "Router 成功重启并继续处理消息 (处理了 $messages_processed 条消息)"
    else
        test_result "$test_name" "FAIL" "Router 重启失败或未恢复消息处理"
    fi
    
    # 收集日志
    docker logs agent-1 --tail 300 > "$TEST_RESULTS_DIR/${test_name}_agent.log" 2>&1
    docker logs router --tail 300 > "$TEST_RESULTS_DIR/${test_name}_router.log" 2>&1
}

# 测试 3: Agent 宕机恢复测试
test_agent_failure_recovery() {
    echo -e "${BLUE}测试 3: Agent 宕机恢复${NC}"
    
    local test_name="agent_failure_recovery"
    local test_duration=180  # 3分钟
    
    # 先启动日志生成器生成一些日志文件
    python3 "$TOOLS_DIR/log_generator.py" \
        --output-dir "/tmp/backpressure-logs-$test_name-pre" \
        --duration 30 \
        --rate "$LOG_RATE" \
        --format json \
        > "$TEST_RESULTS_DIR/${test_name}_pre_log_generator.log" 2>&1
    
    # 等待 Agent 处理一些日志
    sleep 20
    
    # 记录宕机前的消息处理情况
    local messages_before=0
    if docker exec kafka kafka-run-class kafka.tools.GetOffsetShell --broker-list localhost:9092 --topic "$KAFKA_TOPIC" >/dev/null 2>&1; then
        messages_before=$(docker exec kafka kafka-run-class kafka.tools.GetOffsetShell --broker-list localhost:9092 --topic "$KAFKA_TOPIC" | awk -F: '{sum+=$3} END {print sum+0}')
    fi
    
    # 模拟 Agent 宕机
    echo -e "${YELLOW}模拟 Agent 宕机...${NC}"
    docker-compose -f "$COMPOSE_FILE" stop agent-1
    
    # 在 Agent 宕机期间生成更多日志
    python3 "$TOOLS_DIR/log_generator.py" \
        --output-dir "/tmp/backpressure-logs-$test_name-downtime" \
        --duration 60 \
        --rate "$LOG_RATE" \
        --format json \
        > "$TEST_RESULTS_DIR/${test_name}_downtime_log_generator.log" 2>&1 &
    
    local downtime_log_pid=$!
    
    # 等待一段时间确保有足够的新日志
    sleep 40
    
    # 恢复 Agent
    echo -e "${YELLOW}恢复 Agent 服务...${NC}"
    docker-compose -f "$COMPOSE_FILE" start agent-1
    
    # 等待 Agent 重新启动并开始处理
    sleep 30
    
    # 等待日志生成完成
    wait $downtime_log_pid 2>/dev/null || true
    
    # 继续监控一段时间，让 Agent 处理积压的日志
    sleep 60
    
    # 记录恢复后的消息处理情况
    local messages_after=0
    if docker exec kafka kafka-run-class kafka.tools.GetOffsetShell --broker-list localhost:9092 --topic "$KAFKA_TOPIC" >/dev/null 2>&1; then
        messages_after=$(docker exec kafka kafka-run-class kafka.tools.GetOffsetShell --broker-list localhost:9092 --topic "$KAFKA_TOPIC" | awk -F: '{sum+=$3} END {print sum+0}')
    fi
    
    # 检查 Agent 是否成功重启
    local agent_restarted=false
    if docker ps | grep agent-1 >/dev/null; then
        agent_restarted=true
    fi
    
    # 评估测试结果
    local messages_processed=$((messages_after - messages_before))
    
    if [ "$agent_restarted" = true ] && [ $messages_processed -gt 0 ]; then
        test_result "$test_name" "PASS" "Agent 成功重启并继续处理积压日志 (处理了 $messages_processed 条消息)"
    else
        test_result "$test_name" "FAIL" "Agent 重启失败或未恢复日志处理"
    fi
    
    # 收集日志
    docker logs agent-1 --tail 300 > "$TEST_RESULTS_DIR/${test_name}_agent.log" 2>&1
    docker logs router --tail 300 > "$TEST_RESULTS_DIR/${test_name}_router.log" 2>&1
}

# 测试 4: 网络分区恢复测试
test_network_partition() {
    echo -e "${BLUE}测试 4: 网络分区恢复${NC}"
    
    local test_name="network_partition"
    local test_duration=120  # 2分钟
    
    # 启动日志生成器
    python3 "$TOOLS_DIR/log_generator.py" \
        --output-dir "/tmp/backpressure-logs-$test_name" \
        --duration "$test_duration" \
        --rate "$((LOG_RATE / 2))" \
        --format json \
        > "$TEST_RESULTS_DIR/${test_name}_log_generator.log" 2>&1 &
    
    local log_gen_pid=$!
    
    # 等待系统稳定
    sleep 20
    
    # 模拟网络分区 - 阻断 Agent 到 Router 的连接
    echo -e "${YELLOW}模拟网络分区 (阻断 Agent -> Router)...${NC}"
    
    # 获取 Router 容器的网络 IP
    local router_ip=$(docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' router)
    
    # 在 Agent 容器中添加路由规则阻断 Router 连接
    docker exec agent-1 iptables -A OUTPUT -d "$router_ip" -j DROP 2>/dev/null || true
    
    # 等待网络分区生效
    sleep 30
    
    # 恢复网络连接
    echo -e "${YELLOW}恢复网络连接...${NC}"
    docker exec agent-1 iptables -D OUTPUT -d "$router_ip" -j DROP 2>/dev/null || true
    
    # 等待网络恢复和连接重建
    sleep 30
    
    # 等待日志生成器完成
    wait $log_gen_pid 2>/dev/null || true
    
    # 检查 Agent 日志中是否有重连记录
    local reconnection_detected=false
    if docker logs agent-1 --tail 100 2>&1 | grep -i "reconnect\|connection.*lost\|connection.*restored" >/dev/null; then
        reconnection_detected=true
    fi
    
    # 评估测试结果
    if [ "$reconnection_detected" = true ]; then
        test_result "$test_name" "PASS" "成功检测到网络分区和连接恢复"
    else
        test_result "$test_name" "FAIL" "未检测到预期的网络分区恢复行为"
    fi
    
    # 收集日志
    docker logs agent-1 --tail 200 > "$TEST_RESULTS_DIR/${test_name}_agent.log" 2>&1
    docker logs router --tail 200 > "$TEST_RESULTS_DIR/${test_name}_router.log" 2>&1
}

# 生成测试报告
generate_test_report() {
    echo -e "${BLUE}生成测试报告...${NC}"
    
    local report_file="$TEST_RESULTS_DIR/backpressure_test_report.json"
    
    python3 -c "
import json
from datetime import datetime

# 读取测试结果
test_results = []
try:
    with open('$TEST_RESULTS_DIR/test_results.csv') as f:
        lines = f.readlines()
        for line in lines[1:]:  # 跳过标题行
            timestamp, name, result, message = line.strip().split(',', 3)
            test_results.append({
                'timestamp': timestamp,
                'test_name': name,
                'result': result,
                'message': message
            })
except:
    pass

# 生成报告
report = {
    'test_session': '$TIMESTAMP',
    'test_type': 'Backpressure and Recovery Tests',
    'summary': {
        'total_tests': $TESTS_TOTAL,
        'passed_tests': $TESTS_PASSED,
        'failed_tests': $(($TESTS_TOTAL - $TESTS_PASSED)),
        'success_rate': ($TESTS_PASSED * 100 / $TESTS_TOTAL) if $TESTS_TOTAL > 0 else 0
    },
    'test_results': test_results,
    'test_duration': '$TEST_DURATION',
    'log_rate': '$LOG_RATE',
    'kafka_topic': '$KAFKA_TOPIC',
    'recommendations': []
}

# 添加建议
if report['summary']['success_rate'] < 100:
    report['recommendations'].append('部分测试失败，建议检查错误日志并修复相关问题')
if report['summary']['success_rate'] >= 75:
    report['recommendations'].append('系统基本具备反压和故障恢复能力')

# 保存报告
with open('$report_file', 'w') as f:
    json.dump(report, f, indent=2, ensure_ascii=False)

print('测试报告已保存:', '$report_file')
"
    
    # 打印测试总结
    echo ""
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}    反压和故障恢复测试总结${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo "总测试数: $TESTS_TOTAL"
    echo -e "通过测试: ${GREEN}$TESTS_PASSED${NC}"
    echo -e "失败测试: ${RED}$(($TESTS_TOTAL - $TESTS_PASSED))${NC}"
    
    local success_rate=$((TESTS_PASSED * 100 / TESTS_TOTAL))
    echo "成功率: $success_rate%"
    
    if [ -f "$report_file" ]; then
        echo -e "详细报告: ${BLUE}$report_file${NC}"
    fi
    
    echo -e "所有测试日志: ${BLUE}$TEST_RESULTS_DIR${NC}"
}

# 清理测试环境
cleanup_test_environment() {
    echo -e "${BLUE}清理测试环境...${NC}"
    
    cd "$PROJECT_DIR"
    docker-compose -f "$COMPOSE_FILE" down
    
    # 清理临时日志目录
    rm -rf /tmp/backpressure-logs-*
    
    echo -e "${GREEN}✓ 环境清理完成${NC}"
}

# 主函数
main() {
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}    反压和故障恢复测试${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""
    
    # 检查依赖
    if ! command -v docker-compose >/dev/null 2>&1; then
        echo -e "${RED}✗ docker-compose 未安装${NC}"
        exit 1
    fi
    
    if ! command -v python3 >/dev/null 2>&1; then
        echo -e "${RED}✗ python3 未安装${NC}"
        exit 1
    fi
    
    # 创建 CSV 头部
    echo "timestamp,test_name,result,message" > "$TEST_RESULTS_DIR/test_results.csv"
    
    # 设置清理陷阱
    trap cleanup_test_environment EXIT INT TERM
    
    # 启动测试环境
    start_test_environment
    
    # 运行测试序列
    echo -e "${BLUE}开始反压和故障恢复测试序列...${NC}"
    
    test_kafka_throttling
    sleep 30
    
    test_router_failure_recovery
    sleep 30
    
    test_agent_failure_recovery
    sleep 30
    
    test_network_partition
    
    # 生成测试报告
    generate_test_report
    
    echo ""
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}    反压和故障恢复测试完成！${NC}"
    echo -e "${GREEN}========================================${NC}"
    
    # 根据测试结果返回退出码
    if [ $TESTS_PASSED -eq $TESTS_TOTAL ]; then
        exit 0
    else
        exit 1
    fi
}

# 运行主函数
main "$@"
