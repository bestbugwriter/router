#!/bin/bash

# 性能测试脚本
# 测试采集速度、传输速度、写 Kafka 速度

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

# 性能测试配置
PERF_TEST_DURATION=${PERF_TEST_DURATION:-600}  # 10分钟
LOW_RATE=${LOW_RATE:-100}                       # 低负载: 100条/秒
MEDIUM_RATE=${MEDIUM_RATE:-500}                  # 中负载: 500条/秒
HIGH_RATE=${HIGH_RATE:-1000}                     # 高负载: 1000条/秒
PEAK_RATE=${PEAK_RATE:-2000}                     # 峰值负载: 2000条/秒
KAFKA_TOPIC=${KAFKA_TOPIC:-"perf-test-logs"}

# 测试结果目录
RESULTS_DIR="/tmp/performance-test-results"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
TEST_RESULTS_DIR="$RESULTS_DIR/perf_test_$TIMESTAMP"

# 服务配置
COMPOSE_FILE="$PROJECT_DIR/docker-compose.yml"

# 创建测试结果目录
mkdir -p "$TEST_RESULTS_DIR"

# 性能指标收集函数
collect_system_metrics() {
    local test_name=$1
    local duration=$2
    local output_file="$TEST_RESULTS_DIR/${test_name}_system_metrics.csv"
    
    echo -e "${BLUE}收集系统指标: $test_name${NC}"
    
    # 创建 CSV 头部
    echo "timestamp,cpu_percent,memory_used_mb,disk_io_read_mb,disk_io_write_mb,network_rx_mb,network_tx_mb,kafka_messages" > "$output_file"
    
    # 获取初始 Kafka 消息数
    local initial_kafka_messages=0
    if docker exec kafka kafka-run-class kafka.tools.GetOffsetShell --broker-list localhost:9092 --topic "$KAFKA_TOPIC" >/dev/null 2>&1; then
        initial_kafka_messages=$(docker exec kafka kafka-run-class kafka.tools.GetOffsetShell --broker-list localhost:9092 --topic "$KAFKA_TOPIC" | awk -F: '{sum+=$3} END {print sum+0}')
    fi
    
    local start_time=$(date +%s)
    local end_time=$((start_time + duration))
    
    while [ $(date +%s) -lt $end_time ]; do
        local timestamp=$(date +"%Y-%m-%d %H:%M:%S")
        
        # CPU 使用率
        local cpu_percent=$(top -bn1 | grep "Cpu(s)" | awk '{print $2}' | sed 's/%us,//')
        
        # 内存使用
        local memory_used_mb=$(free -m | awk 'NR==2{printf "%.2f", $3}')
        
        # 磁盘 IO (简化版本)
        local disk_io_read_mb=$(vmstat 1 2 | tail -1 | awk '{print $9/1024}')
        local disk_io_write_mb=$(vmstat 1 2 | tail -1 | awk '{print $10/1024}')
        
        # 网络 IO
        local network_stats=$(cat /proc/net/dev | grep eth0 | head -1)
        local network_rx_mb=$(echo $network_stats | awk '{print $2/1024/1024}')
        local network_tx_mb=$(echo $network_stats | awk '{print $10/1024/1024}')
        
        # Kafka 消息数
        local current_kafka_messages=0
        if docker exec kafka kafka-run-class kafka.tools.GetOffsetShell --broker-list localhost:9092 --topic "$KAFKA_TOPIC" >/dev/null 2>&1; then
            current_kafka_messages=$(docker exec kafka kafka-run-class kafka.tools.GetOffsetShell --broker-list localhost:9092 --topic "$KAFKA_TOPIC" | awk -F: '{sum+=$3} END {print sum+0}')
        fi
        
        echo "$timestamp,$cpu_percent,$memory_used_mb,$disk_io_read_mb,$disk_io_write_mb,$network_rx_mb,$network_tx_mb,$((current_kafka_messages - initial_kafka_messages))" >> "$output_file"
        
        sleep 5
    done
    
    echo -e "${GREEN}✓ 系统指标收集完成: $output_file${NC}"
}

# 运行性能测试
run_performance_test() {
    local test_name=$1
    local log_rate=$2
    local duration=$3
    local description=$4
    
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}性能测试: $description${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo "测试名称: $test_name"
    echo "日志速率: $log_rate 条/秒"
    echo "测试时长: $duration 秒"
    
    # 创建测试专用的 Kafka topic
    local test_topic="$KAFKA_TOPIC-$test_name"
    docker exec kafka kafka-topics --create --topic "$test_topic" --bootstrap-server localhost:9092 --partitions 6 --replication-factor 1 --if-not-exists
    
    # 更新 Router 配置使用新的 topic
    sed -i "s/kafka.default.topic=.*/kafka.default.topic=$test_topic/g" "$PROJECT_DIR/config/test-router.properties"
    
    # 重启 Router 以应用新配置
    docker-compose -f "$COMPOSE_FILE" restart router
    sleep 10
    
    # 启动系统指标收集（后台）
    collect_system_metrics "$test_name" "$duration" &
    METRICS_PID=$!
    
    # 启动日志生成器
    echo -e "${BLUE}启动日志生成器 (速率: $log_rate 条/秒)...${NC}"
    python3 "$TOOLS_DIR/log_generator.py" \
        --output-dir "/tmp/perf-test-logs-$test_name" \
        --duration "$duration" \
        --rate "$log_rate" \
        --format json \
        > "$TEST_RESULTS_DIR/${test_name}_log_generator.log" 2>&1 &
    
    LOG_GEN_PID=$!
    
    # 启动 Kafka 验证器
    echo -e "${BLUE}启动 Kafka 性能验证器...${NC}"
    python3 "$TOOLS_DIR/kafka_validator.py" \
        --bootstrap-servers "localhost:9092" \
        --topic "$test_topic" \
        --duration "$((duration - 10))" \
        --output-file "$TEST_RESULTS_DIR/${test_name}_validation_report.json" \
        > "$TEST_RESULTS_DIR/${test_name}_kafka_validator.log" 2>&1 &
    
    KAFKA_VALIDATOR_PID=$!
    
    # 监控测试进度
    local elapsed=0
    while [ $elapsed -lt $duration ]; do
        echo -e "${YELLOW}测试进度: $((elapsed * 100 / duration))% ($elapsed/${duration}秒)${NC}"
        
        # 检查进程状态
        if ! kill -0 $LOG_GEN_PID 2>/dev/null; then
            echo -e "${RED}日志生成器异常退出${NC}"
            break
        fi
        
        if ! kill -0 $KAFKA_VALIDATOR_PID 2>/dev/null; then
            echo -e "${RED}Kafka 验证器异常退出${NC}"
            break
        fi
        
        # 检查容器状态
        if ! docker ps | grep agent-1 >/dev/null || ! docker ps | grep router >/dev/null; then
            echo -e "${RED}Agent 或 Router 容器异常退出${NC}"
            break
        fi
        
        sleep 30
        elapsed=$((elapsed + 30))
    done
    
    # 等待所有进程完成
    echo -e "${BLUE}等待测试完成...${NC}"
    wait $LOG_GEN_PID 2>/dev/null || true
    wait $KAFKA_VALIDATOR_PID 2>/dev/null || true
    kill $METRICS_PID 2>/dev/null || true
    wait $METRICS_PID 2>/dev/null || true
    
    # 收集测试结果
    collect_test_results "$test_name" "$description"
    
    echo -e "${GREEN}✓ 性能测试完成: $test_name${NC}"
}

# 收集测试结果
collect_test_results() {
    local test_name=$1
    local description=$2
    
    echo -e "${BLUE}收集测试结果: $test_name${NC}"
    
    # 收集 Docker 日志
    docker-compose -f "$COMPOSE_FILE" logs agent-1 --tail 1000 > "$TEST_RESULTS_DIR/${test_name}_agent.log" 2>&1
    docker-compose -f "$COMPOSE_FILE" logs router --tail 1000 > "$TEST_RESULTS_DIR/${test_name}_router.log" 2>&1
    
    # 收集容器资源使用情况
    docker stats --no-stream --format "table {{.Container}}\t{{.CPUPerc}}\t{{.MemUsage}}\t{{.NetIO}}\t{{.BlockIO}}" > "$TEST_RESULTS_DIR/${test_name}_container_stats.log"
    
    # 生成测试报告
    local report_file="$TEST_RESULTS_DIR/${test_name}_report.json"
    
    if [ -f "$TEST_RESULTS_DIR/${test_name}_validation_report.json" ]; then
        # 从验证报告中提取性能数据
        python3 -c "
import json
import sys

# 读取验证报告
with open('$TEST_RESULTS_DIR/${test_name}_validation_report.json') as f:
    validation_report = json.load(f)

# 读取系统指标
system_metrics = []
try:
    with open('$TEST_RESULTS_DIR/${test_name}_system_metrics.csv') as f:
        lines = f.readlines()
        headers = lines[0].strip().split(',')
        for line in lines[1:]:
            values = line.strip().split(',')
            system_metrics.append(dict(zip(headers, values)))
except:
    pass

# 生成综合报告
report = {
    'test_name': '$test_name',
    'description': '$description',
    'test_timestamp': '$TIMESTAMP',
    'validation_summary': validation_report.get('validation_summary', {}),
    'performance_metrics': validation_report.get('performance_metrics', {}),
    'system_metrics': system_metrics[-10:] if system_metrics else [],  # 最后10个数据点
    'container_stats': {},
    'recommendations': []
}

# 添加建议
success_rate = validation_report.get('validation_summary', {}).get('success_rate', 0)
throughput = validation_report.get('performance_metrics', {}).get('throughput_msgs_per_sec', 0)

if success_rate < 95:
    report['recommendations'].append('成功率偏低，建议检查网络连接和系统资源')
if throughput < $(echo "$log_rate * 0.8" | bc -l):
    report['recommendations'].append('实际吞吐量低于预期，建议优化配置或增加资源')

# 保存报告
with open('$report_file', 'w') as f:
    json.dump(report, f, indent=2, ensure_ascii=False)

print(f'报告已保存: {report_file}')
"
    fi
    
    echo -e "${GREEN}✓ 测试结果收集完成${NC}"
}

# 生成综合性能报告
generate_performance_summary() {
    echo -e "${BLUE}生成综合性能报告...${NC}"
    
    local summary_file="$TEST_RESULTS_DIR/performance_summary.json"
    
    python3 -c "
import json
import os
import glob

summary = {
    'test_session': '$TIMESTAMP',
    'test_duration': '$PERF_TEST_DURATION',
    'test_results': [],
    'overall_metrics': {
        'total_messages_processed': 0,
        'average_throughput': 0,
        'average_success_rate': 0,
        'peak_throughput': 0
    },
    'recommendations': []
}

# 收集所有测试结果
test_files = glob.glob('$TEST_RESULTS_DIR/*_report.json')
for test_file in test_files:
    try:
        with open(test_file) as f:
            test_report = json.load(f)
            summary['test_results'].append(test_report)
            
            # 累计指标
            validation_summary = test_report.get('validation_summary', {})
            perf_metrics = test_report.get('performance_metrics', {})
            
            summary['overall_metrics']['total_messages_processed'] += validation_summary.get('total_messages', 0)
            summary['overall_metrics']['average_success_rate'] += validation_summary.get('success_rate', 0)
            
            throughput = perf_metrics.get('throughput_msgs_per_sec', 0)
            if throughput > summary['overall_metrics']['peak_throughput']:
                summary['overall_metrics']['peak_throughput'] = throughput
                
    except Exception as e:
        print(f'Error processing {test_file}: {e}')

# 计算平均值
if len(test_files) > 0:
    summary['overall_metrics']['average_success_rate'] /= len(test_files)
    summary['overall_metrics']['average_throughput'] = summary['overall_metrics']['total_messages_processed'] / (len(test_files) * $PERF_TEST_DURATION)

# 生成建议
if summary['overall_metrics']['average_success_rate'] < 95:
    summary['recommendations'].append('整体成功率偏低，建议系统优化')

if summary['overall_metrics']['peak_throughput'] < $HIGH_RATE:
    summary['recommendations'].append('峰值吞吐量未达到预期，建议增加资源或优化配置')

# 保存总结报告
with open('$summary_file', 'w') as f:
    json.dump(summary, f, indent=2, ensure_ascii=False)

print('综合性能报告已保存:', '$summary_file')
"
    
    # 打印总结
    if [ -f "$summary_file" ]; then
        echo -e "${BLUE}性能测试总结:${NC}"
        python3 -c "
import json
with open('$summary_file') as f:
    summary = json.load(f)
metrics = summary['overall_metrics']
print(f'  总处理消息数: {metrics[\"total_messages_processed\"]:,}')
print(f'  平均吞吐量: {metrics[\"average_throughput\"]:.2f} 条/秒')
print(f'  平均成功率: {metrics[\"average_success_rate\"]:.2f}%')
print(f'  峰值吞吐量: {metrics[\"peak_throughput\"]:.2f} 条/秒')
if summary['recommendations']:
    print('  建议:')
    for rec in summary['recommendations']:
        print(f'    - {rec}')
"
    fi
    
    echo -e "${GREEN}✓ 综合性能报告生成完成${NC}"
}

# 启动性能测试环境
start_performance_environment() {
    echo -e "${BLUE}启动性能测试环境...${NC}"
    
    cd "$PROJECT_DIR"
    
    # 启动基础服务
    docker-compose -f "$COMPOSE_FILE" up -d zookeeper kafka control-platform monitor
    
    # 等待服务就绪
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
    
    # 创建测试配置
    cat > "$PROJECT_DIR/config/test-router.properties" << EOF
# Router 性能测试配置
router.id=perf-test-router
router.name=Performance Test Router

# 监听配置
listen.host=0.0.0.0
listen.port=9090
listen.backlog=2048

# IO 线程配置（性能优化）
io.threads=16
session.buffer_size=131072
session.max_connections=2000

# Kafka 配置（性能优化）
kafka.clusters=default
kafka.default.brokers=kafka:29092
kafka.default.topic=$KAFKA_TOPIC
kafka.default.producer.acks=1
kafka.default.producer.retries=0
kafka.default.producer.batch_size=32768
kafka.default.producer.linger_ms=1
kafka.default.producer.compression.type=snappy

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
    
    # 验证服务状态
    if ! docker ps | grep -E "(agent-1|router)" >/dev/null; then
        echo -e "${RED}✗ Agent 或 Router 启动失败${NC}"
        docker-compose -f "$COMPOSE_FILE" logs
        return 1
    fi
    
    echo -e "${GREEN}✓ 性能测试环境启动成功${NC}"
}

# 清理环境
cleanup_performance_environment() {
    echo -e "${BLUE}清理性能测试环境...${NC}"
    
    cd "$PROJECT_DIR"
    docker-compose -f "$COMPOSE_FILE" down
    
    # 清理临时日志目录
    rm -rf /tmp/perf-test-logs-*
    
    echo -e "${GREEN}✓ 环境清理完成${NC}"
}

# 主函数
main() {
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}    Log Pipeline 性能测试${NC}"
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
    
    if ! command -v bc >/dev/null 2>&1; then
        echo -e "${RED}✗ bc 计算器未安装${NC}"
        exit 1
    fi
    
    # 设置清理陷阱
    trap cleanup_performance_environment EXIT INT TERM
    
    # 启动测试环境
    start_performance_environment
    
    # 运行不同负载的性能测试
    echo -e "${BLUE}开始性能测试序列...${NC}"
    
    # 1. 低负载测试
    run_performance_test "low_load" "$LOW_RATE" 120 "低负载测试"
    sleep 30
    
    # 2. 中负载测试
    run_performance_test "medium_load" "$MEDIUM_RATE" 180 "中负载测试"
    sleep 30
    
    # 3. 高负载测试
    run_performance_test "high_load" "$HIGH_RATE" 240 "高负载测试"
    sleep 30
    
    # 4. 峰值负载测试
    run_performance_test "peak_load" "$PEAK_RATE" 300 "峰值负载测试"
    
    # 生成综合报告
    generate_performance_summary
    
    echo ""
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}    性能测试完成！${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo -e "测试结果保存在: ${BLUE}$TEST_RESULTS_DIR${NC}"
}

# 运行主函数
main "$@"
