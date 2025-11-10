# Log Pipeline 测试框架

本目录包含 Log Pipeline 项目的完整测试框架，包括单元测试、集成测试、性能测试和故障恢复测试。

## 目录结构

```
tests/
├── README.md                           # 本文档
├── run_comprehensive_tests.sh          # 综合测试运行脚本
├── run_all_tests.sh                    # 原有测试脚本（向后兼容）
├── run_unit_tests.sh                   # 单元测试脚本
├── TEST_CASES.md                       # 测试用例清单
├── unit/                               # 单元测试
│   ├── test_compressor.cpp
│   └── test_protocol.cpp
├── integration/                        # 集成测试
│   ├── run_integration_tests.sh        # 集成测试主脚本
│   └── test_backpressure_recovery.sh   # 反压和故障恢复测试
├── performance/                        # 性能测试
│   └── run_performance_tests.sh         # 性能测试脚本
└── tools/                              # 测试工具
    ├── log_generator.py                # 日志生成工具
    └── kafka_validator.py              # Kafka 数据验证工具
```

## 快速开始

### 1. 环境准备

确保系统已安装以下依赖：

```bash
# 基本命令
sudo apt-get update
sudo apt-get install -y docker docker-compose python3 gcc make cmake

# Python 库
pip3 install kafka-python psutil

# 项目依赖（用于构建）
sudo apt-get install -y libzookeeper-mt-dev librdkafka-dev zlib1g-dev libsnappy-dev
```

### 2. 运行快速测试

运行单元测试和集成测试（推荐日常开发使用）：

```bash
bash tests/run_comprehensive_tests.sh --quick
```

### 3. 运行完整测试套件

运行所有测试（包括性能测试和故障恢复测试）：

```bash
bash tests/run_comprehensive_tests.sh
```

## 测试类型详解

### 1. 单元测试

测试各个组件的基本功能，包括：
- 协议序列化/反序列化
- 压缩算法
- 配置解析
- 消息队列连接

```bash
# 仅运行单元测试
bash tests/run_comprehensive_tests.sh --unit-only
# 或者
bash tests/run_unit_tests.sh
```

### 2. 集成测试

测试完整的数据流：Agent → Router → Kafka

**测试内容：**
- 自动启动 ZooKeeper、Kafka 等服务
- 生成测试日志
- 验证数据在 Kafka 中的正确性
- 检查数据完整性

```bash
# 仅运行集成测试
bash tests/run_comprehensive_tests.sh --integration-only
# 或者
bash tests/integration/run_integration_tests.sh
```

**环境变量配置：**
```bash
export TEST_DURATION=300    # 测试持续时间（秒）
export LOG_RATE=200         # 日志生成速率（条/秒）
export KAFKA_TOPIC="test-logs"  # Kafka topic 名称
```

### 3. 性能测试

测试系统在不同负载下的性能表现

**测试场景：**
- 低负载：100 条/秒
- 中负载：500 条/秒  
- 高负载：1000 条/秒
- 峰值负载：2000 条/秒

**性能指标：**
- 采集速度
- 传输速度
- 写 Kafka 速度
- 系统资源使用（CPU、内存、网络、磁盘 I/O）

```bash
# 仅运行性能测试
bash tests/run_comprehensive_tests.sh --performance-only
# 或者
bash tests/performance/run_performance_tests.sh
```

**环境变量配置：**
```bash
export PERF_TEST_DURATION=600  # 性能测试持续时间（秒）
export LOW_RATE=100            # 低负载速率
export MEDIUM_RATE=500         # 中负载速率
export HIGH_RATE=1000          # 高负载速率
export PEAK_RATE=2000          # 峰值负载速率
```

### 4. 反压和故障恢复测试

测试系统的容错能力和恢复机制

**测试场景：**
- Kafka 限流导致反压
- Router 宕机恢复
- Agent 宕机恢复
- 网络分区恢复

```bash
# 仅运行反压恢复测试
bash tests/run_comprehensive_tests.sh --backpressure-only
# 或者
bash tests/integration/test_backpressure_recovery.sh
```

## 测试工具

### 1. 日志生成器 (log_generator.py)

生成各种格式的测试日志

```bash
python3 tests/tools/log_generator.py --help

# 示例：生成 JSON 格式日志，持续 60 秒，每秒 200 条
python3 tests/tools/log_generator.py \
    --output-dir /tmp/test-logs \
    --duration 60 \
    --rate 200 \
    --format json
```

**参数：**
- `--output-dir`: 日志输出目录
- `--duration`: 生成持续时间（秒）
- `--rate`: 每秒生成速率（条/秒）
- `--format`: 日志格式（text/json）
- `--no-rotation`: 禁用文件轮转

### 2. Kafka 数据验证器 (kafka_validator.py)

验证 Kafka 中的数据完整性和正确性

```bash
python3 tests/tools/kafka_validator.py --help

# 示例：验证 Kafka topic 中的数据
python3 tests/tools/kafka_validator.py \
    --bootstrap-servers localhost:9092 \
    --topic test-logs \
    --duration 60 \
    --output-file validation_report.json
```

**参数：**
- `--bootstrap-servers`: Kafka bootstrap servers
- `--topic`: Kafka topic 名称
- `--duration`: 验证持续时间（秒）
- `--max-messages`: 最大验证消息数
- `--create-topic`: 如果 topic 不存在则创建
- `--output-file`: 验证报告输出文件

## 测试报告

所有测试结果都会保存在 `/tmp/comprehensive-test-results/` 目录下，按时间戳组织：

```
/tmp/comprehensive-test-results/
└── comprehensive_test_YYYYMMDD_HHMMSS/
    ├── comprehensive_test_report.json    # 综合测试报告
    ├── build.log                         # 构建日志
    ├── unit_tests.log                    # 单元测试日志
    ├── integration_tests.log              # 集成测试日志
    ├── performance_tests.log             # 性能测试日志
    ├── backpressure_tests.log            # 反压测试日志
    ├── integration/                      # 集成测试详细结果
    ├── performance/                      # 性能测试详细结果
    └── backpressure/                     # 反压测试详细结果
```

### 报告解读

**综合测试报告 (comprehensive_test_report.json)：**
```json
{
  "test_session": "20231210_143022",
  "test_type": "Comprehensive Test Suite",
  "summary": {
    "total_suites": 4,
    "passed_suites": 4,
    "failed_suites": 0,
    "success_rate": 100.0
  },
  "test_suites": [...],
  "recommendations": ["所有测试通过，系统质量良好"]
}
```

**性能测试报告：**
- `throughput_msgs_per_sec`: 实际吞吐量（条/秒）
- `success_rate`: 数据完整性成功率（%）
- `average_msg_size`: 平均消息大小（字节）
- 系统资源使用情况

## Docker 集成测试

测试框架使用 Docker Compose 来启动完整的测试环境：

```bash
# 查看测试环境配置
cat docker-compose.yml

# 手动启动测试环境
docker-compose up -d zookeeper kafka control-platform monitor

# 查看服务状态
docker-compose ps

# 查看服务日志
docker-compose logs -f agent-1
docker-compose logs -f router
```

## 故障排除

### 常见问题

1. **Docker 服务启动失败**
   ```bash
   # 检查 Docker 状态
   sudo systemctl status docker
   
   # 检查端口占用
   netstat -tlnp | grep -E "(2181|9092|9090)"
   ```

2. **Kafka 连接失败**
   ```bash
   # 检查 Kafka 是否正常运行
   docker exec kafka kafka-topics --bootstrap-server localhost:9092 --list
   
   # 查看 Kafka 日志
   docker-compose logs kafka
   ```

3. **Python 库缺失**
   ```bash
   # 安装所需库
   pip3 install kafka-python psutil
   
   # 验证安装
   python3 -c "import kafka; import psutil; print('OK')"
   ```

4. **内存不足**
   ```bash
   # 检查系统资源
   free -h
   df -h
   
   # 调整测试参数
   export LOG_RATE=50  # 降低日志生成速率
   export PERF_TEST_DURATION=300  # 缩短测试时间
   ```

### 调试技巧

1. **查看详细日志**
   ```bash
   # 查看特定测试的详细日志
   tail -f /tmp/comprehensive-test-results/*/integration_tests.log
   
   # 查看 Docker 容器日志
   docker-compose logs -f agent-1
   docker-compose logs -f router
   ```

2. **手动验证数据**
   ```bash
   # 检查 Kafka 中的消息
   docker exec kafka kafka-console-consumer \
       --bootstrap-server localhost:9092 \
       --topic test-logs \
       --from-beginning \
       --max-messages 10
   ```

3. **性能分析**
   ```bash
   # 监控系统资源
   htop
   iotop
   iftop
   
   # 监控 Docker 容器资源
   docker stats
   ```

## 持续集成

可以将这些测试集成到 CI/CD 流水线中：

```yaml
# .github/workflows/test.yml
name: Test
on: [push, pull_request]
jobs:
  test:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v2
    - name: Run Tests
      run: |
        bash tests/run_comprehensive_tests.sh --quick
```

## 贡献指南

添加新的测试用例：

1. 单元测试：添加到 `tests/unit/` 目录
2. 集成测试：修改 `tests/integration/run_integration_tests.sh`
3. 性能测试：修改 `tests/performance/run_performance_tests.sh`
4. 更新测试文档：修改 `tests/TEST_CASES.md`

## 联系支持

如果遇到问题或需要帮助，请：
1. 查看本文档的故障排除部分
2. 检查测试日志文件
3. 提交 Issue 到项目仓库
