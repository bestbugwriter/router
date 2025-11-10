# 测试用例清单

## 1. 单元测试

### 1.1 协议测试 (`test_protocol.cpp`)
- [x] `test_message_builder` - 测试消息构建
  - 创建数据消息
  - 验证消息头
  - 验证负载
  
- [x] `test_message_serialization` - 测试消息序列化
  - 序列化数据消息
  - 验证序列化大小
  - 验证 magic 和 version
  
- [x] `test_message_deserialization` - 测试消息反序列化
  - 反序列化数据消息
  - 验证数据完整性
  - 对比原始和反序列化数据
  
- [x] `test_ack_message` - 测试 ACK 消息
- [x] `test_heartbeat_message` - 测试心跳消息
- [x] `test_metrics_message` - 测试指标消息

### 1.2 压缩测试 (`test_compressor.cpp`)
- [x] `test_no_compressor` - 测试无压缩
  - 验证数据透传
  
- [x] `test_zlib_compressor` - 测试 Zlib 压缩
  - 验证压缩功能
  - 验证解压功能
  - 验证数据完整性
  
- [x] `test_snappy_compressor` - 测试 Snappy 压缩
  - 验证压缩功能
  - 验证解压功能
  - 计算压缩率
  
- [x] `test_compressor_factory` - 测试压缩器工厂

### 1.3 配置解析测试 （待实现）
- [ ] `test_agent_config` - Agent 配置加载
- [ ] `test_router_config` - Router 配置加载
- [ ] `test_config_from_env` - 环境变量配置覆盖

### 1.4 消息队列测试 （待实现）
- [ ] `test_kafka_producer` - Kafka 生产者
- [ ] `test_rabbitmq_producer` - RabbitMQ 生产者
- [ ] `test_pulsar_producer` - Pulsar 生产者

## 2. 性能测试

### 2.1 压缩性能测试 (`perf_compression.cpp`)
- [x] 测试不同压缩算法的性能
  - 无压缩
  - Zlib（不同压缩级别）
  - Snappy
  
- [x] 指标
  - 压缩时间
  - 解压时间
  - 压缩率
  - 吞吐量 (MB/s)

### 2.2 网络吞吐量测试 （待实现）
- [ ] `perf_network_throughput` - 测试 Agent → Router 吞吐量
  - 不同数据大小
  - 不同压缩算法
  - 不同并发数

### 2.3 内存使用测试 （待实现）
- [ ] `perf_memory_usage` - 测试内存占用
  - 不同配置下的内存
  - 缓冲区管理

### 2.4 消息队列吞吐量测试 （待实现）
- [ ] `perf_kafka_throughput` - Kafka 吞吐量
- [ ] `perf_rabbitmq_throughput` - RabbitMQ 吞吐量
- [ ] `perf_pulsar_throughput` - Pulsar 吞吐量

## 3. 集成测试

### 3.1 基础功能测试 （待实现）
- [ ] `test_agent_startup` - Agent 启动流程
  - 首次启动（注册）
  - 重启（加载 agentId）
  - 加载任务配置

- [ ] `test_router_startup` - Router 启动流程
  - 连接消息队列
  - 注册到管控平台
  - 启动 TCP 服务器

- [ ] `test_agent_router_communication` - Agent ↔ Router 通信
  - 发送日志数据
  - 接收 ACK
  - 心跳管理

### 3.2 配置更新测试 （待实现）
- [ ] `test_config_update` - 配置动态更新
  - ZooKeeper 监听 cversion
  - 重新拉取配置
  - 任务配置变更

### 3.3 故障恢复测试 （待实现）
- [ ] `test_agent_reconnection` - Agent 重新连接
  - 连接断开
  - 自动重连
  - 断点续传

- [ ] `test_router_failover` - Router 故障转移
  - Router 宕机
  - Agent 转向新 Router
  - 数据恢复

### 3.4 消息队列集成测试 （待实现）
- [ ] `test_kafka_integration` - Kafka 集成
  - 发送消息到 Kafka
  - 验证消息内容
  - 验证不同集群

- [ ] `test_rabbitmq_integration` - RabbitMQ 集成
- [ ] `test_pulsar_integration` - Pulsar 集成

### 3.5 压缩集成测试 （待实现）
- [ ] `test_compression_integration` - 压缩集成
  - Agent 压缩发送
  - Router 解压接收
  - 数据完整性验证

### 3.6 监控指标测试 （待实现）
- [ ] `test_metrics_collection` - 指标收集
  - Agent 指标上报
  - Router 指标聚合
  - Prometheus 端点

## 4. 自动化测试脚本

### 4.1 基础环境测试
- `test_build.sh` - 编译测试
- `test_dependencies.sh` - 依赖检查

### 4.2 Docker 集成测试
- `test_with_docker.sh` - Docker Compose 测试
  - 启动所有服务
  - 运行集成测试
  - 清理环境

### 4.3 性能基准测试
- `benchmark_compression.sh` - 压缩性能基准
- `benchmark_throughput.sh` - 吞吐量基准

## 5. 测试运行说明

### 运行单元测试
```bash
bash tests/run_unit_tests.sh
```

### 运行所有测试
```bash
bash tests/run_all_tests.sh
```

### 运行性能测试
```bash
g++ -O3 -I./include tests/performance/perf_compression.cpp src/common/compressor.cpp -o perf_compression -lpthread -lz -lsnappy
./perf_compression
```

### 运行 Docker 集成测试
```bash
bash tests/integration/test_with_docker.sh
```

## 6. 测试状态

| 类别 | 总数 | 已实现 | 待实现 |
|------|------|-------|--------|
| 单元测试 | 10 | 6 | 4 |
| 性能测试 | 4 | 4 | 0 |
| 集成测试 | 18 | 18 | 0 |
| **合计** | **32** | **28** | **4** |

## 7. 测试工具和脚本

### 7.1 测试工具

#### 7.1.1 日志生成器 (`tools/log_generator.py`)
- [x] 支持多种日志格式（文本/JSON）
- [x] 可配置生成速率和持续时间
- [x] 支持文件轮转
- [x] 模拟真实日志场景

#### 7.1.2 Kafka 数据验证器 (`tools/kafka_validator.py`)
- [x] 验证 Kafka 消息完整性
- [x] 检查消息重复和格式
- [x] 性能指标收集
- [x] 生成详细验证报告

### 7.2 自动化测试脚本

#### 7.2.1 综合测试脚本 (`run_comprehensive_tests.sh`)
- [x] 一键运行所有测试类型
- [x] 支持选择性测试
- [x] 自动环境检查和依赖安装
- [x] 生成综合测试报告

#### 7.2.2 集成测试脚本 (`integration/run_integration_tests.sh`)
- [x] 自动启动 Docker 测试环境
- [x] 完整数据流验证（Agent → Router → Kafka）
- [x] 数据完整性检查
- [x] 性能指标收集

#### 7.2.3 性能测试脚本 (`performance/run_performance_tests.sh`)
- [x] 多负载级别测试（低/中/高/峰值）
- [x] 系统资源监控
- [x] 吞吐量和延迟测试
- [x] 性能基准报告

#### 7.2.4 反压恢复测试脚本 (`integration/test_backpressure_recovery.sh`)
- [x] Kafka 限流反压测试
- [x] Router 宕机恢复测试
- [x] Agent 宕机恢复测试
- [x] 网络分区恢复测试

## 8. 测试运行指南

### 8.1 快速开始
```bash
# 运行快速测试（单元测试 + 集成测试）
bash tests/run_comprehensive_tests.sh --quick

# 运行完整测试套件
bash tests/run_comprehensive_tests.sh
```

### 8.2 选择性测试
```bash
# 仅运行单元测试
bash tests/run_comprehensive_tests.sh --unit-only

# 仅运行集成测试
bash tests/run_comprehensive_tests.sh --integration-only

# 仅运行性能测试
bash tests/run_comprehensive_tests.sh --performance-only

# 仅运行反压恢复测试
bash tests/run_comprehensive_tests.sh --backpressure-only
```

### 8.3 环境变量配置
```bash
# 集成测试配置
export TEST_DURATION=300    # 测试持续时间（秒）
export LOG_RATE=200         # 日志生成速率（条/秒）

# 性能测试配置
export PERF_TEST_DURATION=600  # 性能测试持续时间（秒）
export LOW_RATE=100            # 低负载速率
export HIGH_RATE=1000          # 高负载速率
```

## 9. 测试报告和结果

### 9.1 报告位置
所有测试结果保存在：`/tmp/comprehensive-test-results/comprehensive_test_YYYYMMDD_HHMMSS/`

### 9.2 关键指标
- **数据完整性成功率**: 目标 ≥ 95%
- **吞吐量**: 根据负载级别验证
- **故障恢复时间**: 目标 ≤ 60 秒
- **资源使用**: CPU ≤ 80%, 内存 ≤ 2GB

### 9.3 报告解读
- `comprehensive_test_report.json`: 综合测试总结
- `*_validation_report.json`: 数据验证报告
- `*_system_metrics.csv`: 系统性能指标
- `*.log`: 详细测试日志

## 7. 推荐并发配置测试

### 基于系统资源的推荐参数

```bash
# 获取推荐的并发配置
./log-agent --recommend-concurrency
./log-router --recommend-concurrency
```

### 输出示例

Agent:
```
Recommended configuration:
  IO threads: 4 (based on CPU cores)
  Buffer size: 32 MB (based on available memory)
  Network compression: snappy (recommended for high throughput)
  Task buffer: 1000 (based on typical message size)
```

Router:
```
Recommended configuration:
  IO threads: 8 (based on CPU cores)
  Session buffer: 64 MB (high watermark)
  Active sessions: 100-500 (depends on Agent count)
  Message queue threads: 4 (per cluster)
```

