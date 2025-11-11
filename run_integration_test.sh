#!/bin/bash

# 集成测试主脚本 - 完全容器化版本
# 运行完整的 Agent -> Router -> Kafka 数据流测试

set -o pipefail

# 颜色定义
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# 脚本位置
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"

# 配置
DOCKER_COMPOSE_FILE="$PROJECT_ROOT/docker-compose.test.yml"
RESULTS_DIR="/tmp/integration-test-results-$(date +%Y%m%d_%H%M%S)"
TEST_DURATION=${TEST_DURATION:-60}
LOG_RATE=${LOG_RATE:-200}
KAFKA_TOPIC="test-logs-$(date +%s)"

# 日志函数
log_info() {
    echo -e "${BLUE}[INFO]${NC} $(date '+%Y-%m-%d %H:%M:%S') - $*"
}

log_success() {
    echo -e "${GREEN}[✓]${NC} $(date '+%Y-%m-%d %H:%M:%S') - $*"
}

log_error() {
    echo -e "${RED}[✗]${NC} $(date '+%Y-%m-%d %H:%M:%S') - $*"
}

log_warn() {
    echo -e "${YELLOW}[⚠]${NC} $(date '+%Y-%m-%d %H:%M:%S') - $*"
}

log_step() {
    echo -e "${CYAN}==== $* ====${NC}"
}

# 清理函数
cleanup() {
    log_info "清理资源..."
    
    # 停止并移除容器
    log_info "停止 Docker 容器..."
    docker-compose -f "$DOCKER_COMPOSE_FILE" down --volumes 2>/dev/null || true
    
    log_success "清理完成"
}

# 错误处理
trap cleanup EXIT INT TERM

# 检查依赖
check_dependencies() {
    log_step "检查依赖"
    
    local missing_deps=0
    
    if ! command -v docker >/dev/null 2>&1; then
        log_error "docker 未安装"
        missing_deps=1
    else
        log_success "docker 已安装 ($(docker --version))"
    fi
    
    if ! command -v docker-compose >/dev/null 2>&1; then
        log_error "docker-compose 未安装"
        missing_deps=1
    else
        log_success "docker-compose 已安装 ($(docker-compose --version))"
    fi
    
    if [ $missing_deps -eq 1 ]; then
        log_error "缺少必要的依赖"
        exit 1
    fi
}

# 构建 Docker 镜像
build_images() {
    log_step "构建 Docker 镜像"
    
    log_info "构建 Router 镜像..."
    docker-compose -f "$DOCKER_COMPOSE_FILE" build router 2>&1 | grep -E "(Building|Step|Successfully|ERROR)" || true
    
    if [ $? -ne 0 ]; then
        log_error "Router 镜像构建失败"
        return 1
    fi
    
    log_success "Docker 镜像构建完成"
}

# 启动基础服务
start_base_services() {
    log_step "启动基础服务 (ZooKeeper, Kafka, Control-Platform, Monitor)"
    
    log_info "启动容器..."
    docker-compose -f "$DOCKER_COMPOSE_FILE" up -d zookeeper kafka control-platform monitor 2>&1 | tail -5
    
    # 等待服务启动
    log_info "等待服务启动..."
    sleep 5
    
    # 检查 ZooKeeper
    log_info "检查 ZooKeeper..."
    local zk_ready=0
    for i in {1..30}; do
        if docker exec $(docker-compose -f "$DOCKER_COMPOSE_FILE" ps -q zookeeper) echo ruok | nc localhost 2181 >/dev/null 2>&1; then
            log_success "ZooKeeper 就绪"
            zk_ready=1
            break
        fi
        log_warn "等待 ZooKeeper... ($i/30)"
        sleep 2
    done
    
    if [ $zk_ready -eq 0 ]; then
        log_error "ZooKeeper 启动失败"
        docker-compose -f "$DOCKER_COMPOSE_FILE" logs zookeeper
        return 1
    fi
    
    # 检查 Kafka
    log_info "检查 Kafka..."
    local kafka_ready=0
    for i in {1..30}; do
        if docker-compose -f "$DOCKER_COMPOSE_FILE" exec -T kafka kafka-topics --bootstrap-server localhost:9092 --list >/dev/null 2>&1; then
            log_success "Kafka 就绪"
            kafka_ready=1
            break
        fi
        log_warn "等待 Kafka... ($i/30)"
        sleep 2
    done
    
    if [ $kafka_ready -eq 0 ]; then
        log_error "Kafka 启动失败"
        docker-compose -f "$DOCKER_COMPOSE_FILE" logs kafka
        return 1
    fi
    
    # 创建测试 topic
    log_info "创建 Kafka Topic: $KAFKA_TOPIC"
    docker-compose -f "$DOCKER_COMPOSE_FILE" exec -T kafka kafka-topics \
        --create \
        --topic "$KAFKA_TOPIC" \
        --bootstrap-server localhost:9092 \
        --partitions 3 \
        --replication-factor 1 \
        --if-not-exists 2>&1 | tail -3
    
    log_success "基础服务启动完成"
}

# 启动 Router
start_router() {
    log_step "启动 Router"
    
    log_info "启动 Router 容器..."
    docker-compose -f "$DOCKER_COMPOSE_FILE" up -d router 2>&1 | tail -5
    
    # 等待 Router 启动
    log_info "等待 Router 启动..."
    local router_ready=0
    for i in {1..30}; do
        if docker-compose -f "$DOCKER_COMPOSE_FILE" exec -T router curl -f http://localhost:9101/health >/dev/null 2>&1; then
            log_success "Router 就绪 (9101/health)"
            router_ready=1
            break
        fi
        log_warn "等待 Router... ($i/30)"
        sleep 2
    done
    
    if [ $router_ready -eq 0 ]; then
        log_error "Router 启动失败"
        docker-compose -f "$DOCKER_COMPOSE_FILE" logs router | tail -50
        return 1
    fi
    
    log_success "Router 启动完成"
}

# 运行日志生成测试
run_log_generation_test() {
    log_step "运行日志生成测试"
    
    mkdir -p "$RESULTS_DIR"
    
    log_info "使用容器化的日志生成器..."
    log_info "参数:"
    log_info "  - 持续时间: ${TEST_DURATION}秒"
    log_info "  - 生成速率: ${LOG_RATE}条/秒"
    log_info "  - 输出格式: json"
    
    # 使用 docker 运行日志生成器
    docker run --rm \
        --network log-pipeline-test \
        -e PYTHONUNBUFFERED=1 \
        --entrypoint /usr/local/bin/python \
        $(docker-compose -f "$DOCKER_COMPOSE_FILE" config --services | grep -E "router|zookeeper" | head -1 | xargs -I {} docker-compose -f "$DOCKER_COMPOSE_FILE" images {} | awk 'NR==2 {print $1}') \
        /app/tools/log_generator.py \
        --output-dir /tmp \
        --duration "$TEST_DURATION" \
        --rate "$LOG_RATE" \
        --format json \
        2>&1 | tee "$RESULTS_DIR/log_generator.log" &
    
    LOG_GEN_PID=$!
    log_success "日志生成器已启动 (PID: $LOG_GEN_PID)"
    
    # 等待一段时间
    sleep 10
    
    # 运行 Kafka 验证器
    log_info "启动 Kafka 数据验证..."
    docker run --rm \
        --network log-pipeline-test \
        -e PYTHONUNBUFFERED=1 \
        --entrypoint /usr/local/bin/python \
        $(docker-compose -f "$DOCKER_COMPOSE_FILE" images router | awk 'NR==2 {print $1}') \
        /app/tools/kafka_validator.py \
        --bootstrap-servers "kafka:29092" \
        --topic "$KAFKA_TOPIC" \
        --duration "$((TEST_DURATION - 20))" \
        --output-file /tmp/validation_report.json \
        2>&1 | tee "$RESULTS_DIR/kafka_validator.log" &
    
    KAFKA_VAL_PID=$!
    log_success "Kafka 验证器已启动 (PID: $KAFKA_VAL_PID)"
    
    # 监控进度
    local elapsed=0
    while [ $elapsed -lt $TEST_DURATION ]; do
        log_info "测试进度: $((elapsed * 100 / TEST_DURATION))% ($elapsed/${TEST_DURATION}秒)"
        
        if ! kill -0 $LOG_GEN_PID 2>/dev/null; then
            log_warn "日志生成器已停止"
        fi
        
        if ! kill -0 $KAFKA_VAL_PID 2>/dev/null; then
            log_warn "Kafka 验证器已停止"
        fi
        
        sleep 10
        elapsed=$((elapsed + 10))
    done
    
    # 等待进程完成
    log_info "等待测试完成..."
    wait $LOG_GEN_PID 2>/dev/null || true
    wait $KAFKA_VAL_PID 2>/dev/null || true
    
    log_success "日志生成测试完成"
}

# 收集结果和日志
collect_results() {
    log_step "收集测试结果"
    
    log_info "保存目录: $RESULTS_DIR"
    
    # 收集容器日志
    mkdir -p "$RESULTS_DIR/logs"
    
    log_info "收集 Router 日志..."
    docker-compose -f "$DOCKER_COMPOSE_FILE" logs router > "$RESULTS_DIR/logs/router.log" 2>&1 || true
    
    log_info "收集 Kafka 日志..."
    docker-compose -f "$DOCKER_COMPOSE_FILE" logs kafka > "$RESULTS_DIR/logs/kafka.log" 2>&1 || true
    
    log_info "收集 ZooKeeper 日志..."
    docker-compose -f "$DOCKER_COMPOSE_FILE" logs zookeeper > "$RESULTS_DIR/logs/zookeeper.log" 2>&1 || true
    
    # 收集系统信息
    log_info "收集系统信息..."
    {
        echo "=== Docker 容器状态 ==="
        docker-compose -f "$DOCKER_COMPOSE_FILE" ps
        echo ""
        echo "=== Kafka Topic 信息 ==="
        docker-compose -f "$DOCKER_COMPOSE_FILE" exec -T kafka kafka-topics --describe --topic "$KAFKA_TOPIC" --bootstrap-server localhost:9092 2>&1 || echo "Topic 不存在"
        echo ""
        echo "=== 消息计数 ==="
        docker-compose -f "$DOCKER_COMPOSE_FILE" exec -T kafka kafka-console-consumer \
            --bootstrap-server localhost:9092 \
            --topic "$KAFKA_TOPIC" \
            --from-beginning \
            --max-messages 5 \
            --timeout-ms 10000 2>&1 | wc -l || echo "无消息或超时"
    } > "$RESULTS_DIR/system_info.log"
    
    log_success "结果收集完成"
}

# 生成报告
generate_report() {
    log_step "生成测试报告"
    
    cat > "$RESULTS_DIR/test_report.txt" << EOF
========================================
集成测试报告
========================================

测试时间: $(date '+%Y-%m-%d %H:%M:%S')
测试持续时间: ${TEST_DURATION}秒
日志生成速率: ${LOG_RATE}条/秒
Kafka Topic: $KAFKA_TOPIC

结果目录: $RESULTS_DIR

包含的日志:
- logs/router.log - Router 容器日志
- logs/kafka.log - Kafka 容器日志
- logs/zookeeper.log - ZooKeeper 容器日志
- system_info.log - 系统信息
- log_generator.log - 日志生成器输出
- kafka_validator.log - Kafka 验证器输出

查看完整日志:
  cat $RESULTS_DIR/logs/router.log
  
查看系统信息:
  cat $RESULTS_DIR/system_info.log

清理结果目录:
  rm -rf $RESULTS_DIR

========================================
EOF
    
    cat "$RESULTS_DIR/test_report.txt"
}

# 主函数
main() {
    echo ""
    log_step "Log Pipeline 集成测试"
    echo ""
    
    # 检查前提
    check_dependencies || exit 1
    
    # 构建镜像
    build_images || {
        log_error "构建失败"
        exit 1
    }
    
    # 启动服务
    start_base_services || {
        log_error "基础服务启动失败"
        exit 1
    }
    
    start_router || {
        log_error "Router 启动失败"
        exit 1
    }
    
    # 运行测试
    run_log_generation_test || {
        log_error "测试执行失败"
        exit 1
    }
    
    # 收集结果
    collect_results
    generate_report
    
    echo ""
    log_success "集成测试完成！"
    log_info "结果目录: $RESULTS_DIR"
    echo ""
}

# 运行主函数
main "$@"
