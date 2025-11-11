#!/bin/bash

# 完整的集成测试脚本
# 测试 Agent -> Router -> Kafka 的完整数据流

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

# 配置变量
TEST_DURATION=${TEST_DURATION:-300}  # 5分钟测试
LOG_RATE=${LOG_RATE:-200}           # 每秒200条日志
KAFKA_TOPIC=${KAFKA_TOPIC:-"test-logs-$(date +%s)"}
COMPOSE_FILE="$PROJECT_DIR/docker-compose.test.yml"
LOG_DIR="/tmp/integration-test-logs"
RESULTS_DIR="/tmp/integration-test-results-$(date +%Y%m%d_%H%M%S)"

# Docker Compose 命令兼容性处理
if command -v docker-compose >/dev/null 2>&1; then
    DOCKER_COMPOSE_CMD="docker-compose"
else
    DOCKER_COMPOSE_CMD="docker compose"
fi

# 服务健康检查函数
check_service_health() {
    local service_name=$1
    local health_check_url=$2
    local max_attempts=${3:-30}
    local attempt=1
    
    echo -e "${BLUE}检查 $service_name 健康状态...${NC}"
    
    while [ $attempt -le $max_attempts ]; do
        if curl -s "$health_check_url" >/dev/null 2>&1; then
            echo -e "${GREEN}✓ $service_name 健康检查通过${NC}"
            return 0
        fi
        
        echo -e "${YELLOW}等待 $service_name 启动... (尝试 $attempt/$max_attempts)${NC}"
        sleep 2
        ((attempt++))
    done
    
    echo -e "${RED}✗ $service_name 健康检查失败${NC}"
    return 1
}

# 检查 Kafka 是否就绪
check_kafka_ready() {
    local max_attempts=${1:-30}
    local attempt=1
    
    echo -e "${BLUE}检查 Kafka 是否就绪...${NC}"
    
    while [ $attempt -le $max_attempts ]; do
        if docker exec kafka kafka-topics --bootstrap-server localhost:9092 --list >/dev/null 2>&1; then
            echo -e "${GREEN}✓ Kafka 已就绪${NC}"
            return 0
        fi
        
        echo -e "${YELLOW}等待 Kafka 启动... (尝试 $attempt/$max_attempts)${NC}"
        sleep 2
        ((attempt++))
    done
    
    echo -e "${RED}✗ Kafka 启动失败${NC}"
    return 1
}

# 创建测试配置文件
create_test_configs() {
    echo -e "${BLUE}创建测试配置文件...${NC}"
    
    # 创建 Agent 配置
    cat > "$PROJECT_DIR/config/test-agent.properties" << EOF
# Agent 测试配置
agent.id=test-agent-001
agent.name=Integration Test Agent

# ZooKeeper 配置
zookeeper.hosts=zookeeper:2181
zookeeper.session_timeout=30000
zookeeper.connection_timeout=10000

# Router 配置
router.servers=router:9090
router.connection_timeout=10000
router.reconnect_interval=5000

# 文件监控配置
file.monitor.directories=/var/log/test
file.monitor.patterns=*.log,*.json
file.monitor.exclude_patterns=*.tmp,*.swp
file.monitor.scan_interval=1000

# 任务配置
task.buffer_size=10000
task.batch_size=100
task.flush_interval=1000

# 网络配置
network.io_threads=4
network.compression=snappy
network.connect_timeout=10000
network.send_timeout=5000

# 指标配置
metrics.enabled=true
metrics.interval=10000
metrics.port=8081
EOF

    # 创建 Router 配置
    cat > "$PROJECT_DIR/config/test-router.properties" << EOF
# Router 测试配置
router.id=test-router-001
router.name=Integration Test Router

# 监听配置
listen.host=0.0.0.0
listen.port=9090
listen.backlog=1024

# IO 线程配置
io.threads=8
session.buffer_size=65536
session.max_connections=1000

# ZooKeeper 配置
zookeeper.hosts=zookeeper:2181
zookeeper.session_timeout=30000

# Kafka 集群配置
kafka.clusters=default
kafka.default.brokers=kafka:29092
kafka.default.topic=$KAFKA_TOPIC
kafka.default.producer.acks=1
kafka.default.producer.retries=3
kafka.default.producer.batch_size=16384
kafka.default.producer.linger_ms=5

# 管控平台配置
control.platform.host=control-platform
control.platform.port=80
control.platform.register_interval=30000

# 监控配置
monitor.host=monitor
monitor.port=80
monitor.enabled=true
monitor.interval=10000

# 压缩配置
compression.enabled=true
compression.algorithm=snappy

# 指标配置
metrics.enabled=true
metrics.port=9101
metrics.interval=10000
EOF

    echo -e "${GREEN}✓ 测试配置文件创建完成${NC}"
}

# 启动测试环境
start_test_environment() {
    echo -e "${BLUE}启动测试环境...${NC}"
    
    # 创建必要的目录
    mkdir -p "$LOG_DIR" "$RESULTS_DIR"
    
    # 启动 Docker Compose 服务
    cd "$PROJECT_DIR"
    $DOCKER_COMPOSE_CMD -f "$COMPOSE_FILE" up -d zookeeper kafka control-platform monitor
    
    # 等待基础服务启动
    echo -e "${YELLOW}等待基础服务启动...${NC}"
    sleep 10
    
    # 检查服务健康状态
    check_service_health "ZooKeeper" "http://localhost:2181" 30
    check_kafka_ready 30
    check_service_health "Control Platform" "http://localhost:8080" 20
    check_service_health "Monitor" "http://localhost:9200" 20
    
    # 创建测试 topic
    echo -e "${BLUE}创建 Kafka topic: $KAFKA_TOPIC${NC}"
    docker exec kafka kafka-topics --create --topic "$KAFKA_TOPIC" --bootstrap-server localhost:9092 --partitions 3 --replication-factor 1 --if-not-exists
    
    # 启动 Agent 和 Router
    echo -e "${BLUE}启动 Agent 和 Router...${NC}"
    
    # 更新 docker-compose 使用测试配置
    sed -i.bak "s|/etc/log-pipeline/agent.properties|/etc/log-pipeline/test-agent.properties|g" "$COMPOSE_FILE"
    sed -i.bak "s|/etc/log-pipeline/router.properties|/etc/log-pipeline/test-router.properties|g" "$COMPOSE_FILE"
    
    $DOCKER_COMPOSE_CMD -f "$COMPOSE_FILE" up -d agent-1 router
    
    # 等待 Agent 和 Router 启动
    sleep 15
    
    # 检查 Agent 和 Router 是否正常运行
    if ! docker ps | grep agent-1 >/dev/null; then
        echo -e "${RED}✗ Agent 启动失败${NC}"
        $DOCKER_COMPOSE_CMD -f "$COMPOSE_FILE" logs agent-1
        return 1
    fi
    
    if ! docker ps | grep router >/dev/null; then
        echo -e "${RED}✗ Router 启动失败${NC}"
        $DOCKER_COMPOSE_CMD -f "$COMPOSE_FILE" logs router
        return 1
    fi
    
    echo -e "${GREEN}✓ 测试环境启动成功${NC}"
}

# 运行集成测试
run_integration_test() {
    echo -e "${BLUE}====== 开始集成测试 ======${NC}"
    echo "测试参数:"
    echo "  持续时间: ${TEST_DURATION}秒"
    echo "  生成速率: ${LOG_RATE}条/秒"
    echo "  Kafka Topic: $KAFKA_TOPIC"
    echo "  结果目录: $RESULTS_DIR"
    echo ""
    
    # 创建结果目录
    mkdir -p "$RESULTS_DIR"
    
    # 在后台生成日志
    echo -e "${BLUE}启动日志生成器...${NC}"
    echo "命令: python3 $TOOLS_DIR/log_generator.py \\"
    echo "  --output-dir $LOG_DIR \\"
    echo "  --duration $TEST_DURATION \\"
    echo "  --rate $LOG_RATE \\"
    echo "  --format json"
    echo ""
    
    python3 "$TOOLS_DIR/log_generator.py" \
        --output-dir "$LOG_DIR" \
        --duration "$TEST_DURATION" \
        --rate "$LOG_RATE" \
        --format json \
        2>&1 | tee "$RESULTS_DIR/log_generator.log" &
    
    LOG_GENERATOR_PID=$!
    echo -e "${GREEN}✓ 日志生成器已启动 (PID: $LOG_GENERATOR_PID)${NC}"
    echo ""
    
    # 等待一段时间让系统开始处理日志
    echo -e "${YELLOW}等待系统处理日志...${NC}"
    sleep 10
    
    # 启动 Kafka 验证器
    echo -e "${BLUE}启动 Kafka 数据验证器...${NC}"
    echo "命令: python3 $TOOLS_DIR/kafka_validator.py \\"
    echo "  --bootstrap-servers localhost:9092 \\"
    echo "  --topic $KAFKA_TOPIC \\"
    echo "  --duration $((TEST_DURATION - 20)) \\"
    echo "  --output-file $RESULTS_DIR/validation_report.json"
    echo ""
    
    python3 "$TOOLS_DIR/kafka_validator.py" \
        --bootstrap-servers "localhost:9092" \
        --topic "$KAFKA_TOPIC" \
        --duration "$((TEST_DURATION - 20))" \
        --output-file "$RESULTS_DIR/validation_report.json" \
        2>&1 | tee "$RESULTS_DIR/kafka_validator.log" &
    
    KAFKA_VALIDATOR_PID=$!
    echo -e "${GREEN}✓ Kafka 验证器已启动 (PID: $KAFKA_VALIDATOR_PID)${NC}"
    echo ""
    
    # 监控测试进度
    local elapsed=0
    while [ $elapsed -lt $TEST_DURATION ]; do
        local progress=$((elapsed * 100 / TEST_DURATION))
        echo -e "${BLUE}[测试进度] $progress% | $elapsed/${TEST_DURATION}秒${NC}"
        
        # 检查进程是否还在运行
        if ! kill -0 $LOG_GENERATOR_PID 2>/dev/null; then
            echo -e "${YELLOW}日志生成器已停止${NC}"
        fi
        
        if ! kill -0 $KAFKA_VALIDATOR_PID 2>/dev/null; then
            echo -e "${YELLOW}Kafka 验证器已停止${NC}"
        fi
        
        sleep 10
        elapsed=$((elapsed + 10))
    done
    
    # 等待进程完成
    echo -e "${BLUE}等待测试完成...${NC}"
    wait $LOG_GENERATOR_PID 2>/dev/null || true
    wait $KAFKA_VALIDATOR_PID 2>/dev/null || true
    
    echo -e "${GREEN}✓ 集成测试完成${NC}"
}

# 收集测试结果
collect_test_results() {
    echo ""
    echo -e "${BLUE}====== 收集测试结果 ======${NC}"
    echo ""
    
    # 收集 Docker 日志
    mkdir -p "$RESULTS_DIR/logs"
    
    echo -e "${BLUE}收集 Router 日志...${NC}"
    $DOCKER_COMPOSE_CMD -f "$COMPOSE_FILE" logs router > "$RESULTS_DIR/logs/router.log" 2>&1
    echo "  保存到: $RESULTS_DIR/logs/router.log"
    
    echo -e "${BLUE}收集 Kafka 日志...${NC}"
    $DOCKER_COMPOSE_CMD -f "$COMPOSE_FILE" logs kafka > "$RESULTS_DIR/logs/kafka.log" 2>&1
    echo "  保存到: $RESULTS_DIR/logs/kafka.log"
    
    echo -e "${BLUE}收集 ZooKeeper 日志...${NC}"
    $DOCKER_COMPOSE_CMD -f "$COMPOSE_FILE" logs zookeeper > "$RESULTS_DIR/logs/zookeeper.log" 2>&1
    echo "  保存到: $RESULTS_DIR/logs/zookeeper.log"
    
    # 统计生成的日志文件
    local log_files_count=$(find "$LOG_DIR" -name "*.log" -o -name "*.json" | wc -l)
    local total_log_size=$(du -sh "$LOG_DIR" 2>/dev/null | cut -f1 || echo "0")
    
    echo -e "${BLUE}测试统计:${NC}"
    echo "  日志文件数量: $log_files_count"
    echo "  日志总大小: $total_log_size"
    
    # 显示验证报告
    if [ -f "$RESULTS_DIR/validation_report.json" ]; then
        echo -e "${BLUE}Kafka 验证报告:${NC}"
        python3 -c "
import json
with open('$RESULTS_DIR/validation_report.json') as f:
    report = json.load(f)
summary = report['validation_summary']
print(f'  总消息数: {summary[\"total_messages\"]}')
print(f'  有效消息: {summary[\"valid_messages\"]}')
print(f'  无效消息: {summary[\"invalid_messages\"]}')
print(f'  成功率: {summary[\"success_rate\"]:.2f}%')
perf = report['performance_metrics']
print(f'  吞吐量: {perf[\"throughput_msgs_per_sec\"]:.2f} 条/秒')
"
    fi
    
    # 生成测试报告
    echo ""
    echo -e "${BLUE}生成测试报告...${NC}"
    
    cat > "$RESULTS_DIR/test_report.txt" << REPORT_EOF
========================================
集成测试报告
========================================

生成时间: $(date '+%Y-%m-%d %H:%M:%S')

测试配置:
  持续时间: ${TEST_DURATION}秒
  日志速率: ${LOG_RATE}条/秒
  Kafka Topic: $KAFKA_TOPIC

结果统计:
  日志文件数量: $log_files_count
  日志总大小: $total_log_size

结果目录: $RESULTS_DIR

日志文件:
  - logs/router.log
  - logs/kafka.log
  - logs/zookeeper.log
  - log_generator.log
  - kafka_validator.log

查看日志:
  cat $RESULTS_DIR/logs/router.log
  cat $RESULTS_DIR/logs/kafka.log
  cat $RESULTS_DIR/kafka_validator.log

验证报告:
  cat $RESULTS_DIR/validation_report.json

========================================
REPORT_EOF
    
    echo "  保存到: $RESULTS_DIR/test_report.txt"
    echo ""
    echo -e "${GREEN}✓ 测试结果已保存到: $RESULTS_DIR${NC}"
}

# 清理测试环境
cleanup_test_environment() {
    echo -e "${BLUE}清理测试环境...${NC}"
    
    # 停止 Docker 服务
    cd "$PROJECT_DIR"
    $DOCKER_COMPOSE_CMD -f "$COMPOSE_FILE" down
    
    # 恢复原始配置
    if [ -f "$COMPOSE_FILE.bak" ]; then
        mv "$COMPOSE_FILE.bak" "$COMPOSE_FILE"
    fi
    
    # 清理临时文件
    rm -rf "$LOG_DIR"
    
    echo -e "${GREEN}✓ 测试环境清理完成${NC}"
}

# 主函数
main() {
    echo ""
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}    Log Pipeline 集成测试 - 开始${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""
    
    # 检查依赖
    echo -e "${BLUE}检查依赖...${NC}"
    if command -v docker-compose >/dev/null 2>&1; then
        echo -e "${GREEN}✓ docker-compose 已安装${NC}"
    elif docker compose version >/dev/null 2>&1; then
        echo -e "${GREEN}✓ docker compose 已安装${NC}"
    else
        echo -e "${RED}✗ docker-compose 或 docker compose 未安装${NC}"
        exit 1
    fi
    
    if ! command -v python3 >/dev/null 2>&1; then
        echo -e "${RED}✗ python3 未安装${NC}"
        exit 1
    fi
    echo -e "${GREEN}✓ python3 已安装${NC}"
    
    # 检查 Python 依赖
    if ! python3 -c "import kafka" 2>/dev/null; then
        echo -e "${YELLOW}⚠ kafka-python 库未安装，尝试安装...${NC}"
        pip3 install kafka-python || {
            echo -e "${RED}✗ 安装 kafka-python 失败${NC}"
            exit 1
        }
    fi
    
    # 设置清理陷阱
    trap cleanup_test_environment EXIT INT TERM
    
    # 执行测试步骤
    create_test_configs
    start_test_environment
    run_integration_test
    collect_test_results
    
    echo ""
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}    集成测试完成！${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo ""
    echo -e "✓ 测试结果保存在:"
    echo -e "  ${BLUE}$RESULTS_DIR${NC}"
    echo ""
    echo "快速查看结果:"
    echo "  cat $RESULTS_DIR/test_report.txt"
    echo "  cat $RESULTS_DIR/logs/router.log"
    echo "  cat $RESULTS_DIR/kafka_validator.log"
    echo ""
}

# 运行主函数
main "$@"
