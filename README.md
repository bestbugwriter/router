# C++ 日志采集与路由系统

高性能的 C++ 日志采集（Agent）与路由（Router）系统，支持多消息队列、网络压缩、分布式配置管理和监控。

## 主要特性

- **高性能异步I/O**: 基于 Asio 的异步网络通信，支持高并发
- **多消息队列支持**: Kafka（多版本）、RabbitMQ、Apache Pulsar
- **网络数据压缩**: Zlib、Snappy 压缩算法，可配置
- **分布式配置管理**: 管控平台集中管理任务配置，支持热更新
- **ZooKeeper 集成**: Agent 注册和状态监控
- **完整的监控指标**: Prometheus 格式指标暴露，支持上报到监控服务
- **Docker 容器化**: 提供 Dockerfile 和 docker-compose，开箱即用
- **静态编译支持**: 可编译成完全独立的静态二进制文件

## 架构概览

### 系统组件

```
┌─────────────────────────────────────────────────────────────────┐
│                       管控平台 (Control Plane)                   │
│           负责：任务配置、Agent注册、监控管理                     │
└─────────────────────────────────────────────────────────────────┘
          ▲                              ▲
          │                              │
          │ Pull Config                  │ Register/Report Status
          │ (cversion)                   │
          │                              │
    ┌─────┴────────┐        ┌───────────┴──────────┐
    │    Agent     │        │  ZooKeeper Registry  │
    │              │◄──────►│  (Registration,      │
    │  ┌────────┐  │ Watch  │   Status Monitoring) │
    │  │ Tailer │  │ Tasks  │                      │
    │  └────────┘  │        └──────────────────────┘
    │  ┌────────┐  │
    │  │TCP CLI │──┼─────────────┐
    │  └────────┘  │             │
    │  ┌────────┐  │             │ TCP + Compression
    │  │Metrics │  │             │ (Optional: zlib, snappy)
    │  └────────┘  │             │
    └──────────────┘             │
                                 ▼
                        ┌──────────────────┐
                        │     Router       │
                        │  ┌────────────┐  │
                        │  │Decompress  │  │
                        │  │Route by MQ │  │
                        │  └────────────┘  │
                        │  ┌────────────┐  │
                        │  │ Producers  │  │
                        │  │ (Kafka/    │  │
                        │  │  RabbitMQ/ │  │
                        │  │  Pulsar)   │  │
                        │  └────────────┘  │
                        └────────┬─────────┘
                                 │
                ┌────────────────┼────────────────┐
                ▼                ▼                ▼
          ┌──────────┐      ┌──────────┐    ┌──────────┐
          │ Kafka    │      │RabbitMQ  │    │ Pulsar   │
          │Cluster   │      │ Cluster  │    │ Cluster  │
          └──────────┘      └──────────┘    └──────────┘
```

## 技术栈

### 核心库
- **Asio**: 异步 I/O 库（standalone 版本）
- **nlohmann/json**: JSON 处理
- **spdlog**: 日志记录库
- **inih**: INI 配置文件解析

### 消息队列客户端
- **librdkafka++**: Kafka 客户端库
- **RabbitMQ C++ Client**: RabbitMQ 客户端（可选）
- **Pulsar C++ Client**: Pulsar 客户端（可选）

### 压缩库
- **zlib**: Zlib 压缩算法（可选）
- **snappy**: Snappy 压缩算法（可选）

### 监控
- **prometheus-cpp**: Prometheus 指标导出（可选）

## 快速开始

### 依赖安装

#### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install -y \
    cmake \
    build-essential \
    libzookeeper-mt-dev \
    librdkafka-dev \
    zlib1g-dev \
    libsnappy-dev \
    pkg-config
```

#### CentOS/RHEL
```bash
sudo yum install -y \
    cmake \
    gcc-c++ \
    make \
    zookeeper-devel \
    librdkafka-devel \
    zlib-devel \
    snappy-devel \
    pkgconfig
```

### 编译

#### 动态链接
```bash
bash build.sh
```

#### 静态链接（推荐用于容器化）
```bash
bash build.sh --static
```

#### 调试版本
```bash
bash build.sh --debug
```

编译后的二进制文件位于 `build/` 目录：
- `log-agent` - 日志采集程序
- `log-router` - 日志路由程序（替代原有的 log-forwarder）

### 配置

#### Agent 配置 (`config/agent.properties`)

```properties
# 基础配置
agent.id = agent-001
agent.name = log-agent-1

# Router 地址（可多个）
agent.forwarders = 127.0.0.1:9090,127.0.0.1:9091

# 本地文件
agent.checkpoint_file_path = /var/lib/log-agent/offsets.json

# 管控平台
control.platform.host = 127.0.0.1
control.platform.port = 8080

# ZooKeeper（仅用于注册和监控）
zookeeper.hosts = 127.0.0.1:2181
zookeeper.agent_status_path = /log-pipeline/agents
zookeeper.watch_path = /log-pipeline/configs

# 网络传输
network.compression = snappy  # none, zlib, snappy
network.buffer_size_mb = 10

# 指标
metrics.report_interval_sec = 10
```

#### Router 配置 (`config/router.properties`)

```properties
# 基础配置
router.id = router-001

# 服务配置
server.listen_host = 0.0.0.0
server.listen_port = 9090
server.io_threads = 8

# 管控平台
control.platform.host = 127.0.0.1
control.platform.port = 8080

# Kafka 集群（支持多个不同版本）
mq.kafka.cluster.kafka-cluster-1.brokers = 127.0.0.1:9092
mq.kafka.cluster.kafka-cluster-1.version = 3.x
mq.kafka.cluster.kafka-cluster-1.props.compression.type = snappy

# RabbitMQ 集群
# mq.rabbitmq.cluster.rabbit-1.hosts = 127.0.0.1:5672
# mq.rabbitmq.cluster.rabbit-1.version = 4.x

# Pulsar 集群
# mq.pulsar.cluster.pulsar-1.url = pulsar://127.0.0.1:6650
# mq.pulsar.cluster.pulsar-1.version = 3.x

# 反压
backpressure.session_buffer_high_watermark_mb = 64
backpressure.session_buffer_low_watermark_mb = 32

# 指标
metrics.prometheus_listen = 0.0.0.0:9101

# Monitor 服务
monitor.host = 127.0.0.1
monitor.port = 9200
```

### 运行

```bash
# 启动 Agent
./build/log-agent config/agent.properties

# 启动 Router
./build/log-router config/router.properties
```

## Docker 部署

### 使用 docker-compose

```bash
# 启动所有服务（包括 ZooKeeper、Kafka 等）
docker-compose up -d

# 查看日志
docker-compose logs -f

# 停止服务
docker-compose down
```

### 单独构建和运行

```bash
# 构建 Agent 镜像
docker build -t log-agent:latest --target agent .

# 构建 Router 镜像
docker build -t log-router:latest --target router .

# 运行 Agent
docker run -d \
  -e AGENT_ID=agent-001 \
  -e ZOOKEEPER_HOSTS=zookeeper:2181 \
  -v /path/to/logs:/var/log/app \
  -v agent-data:/var/lib/log-agent \
  log-agent:latest

# 运行 Router
docker run -d \
  -e ROUTER_ID=router-001 \
  -e KAFKA_BROKERS=kafka:9092 \
  -p 9090:9090 \
  -p 9101:9101 \
  log-router:latest
```

## 测试

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
g++ -O3 -I./include tests/performance/perf_compression.cpp src/common/compressor.cpp \
    -o perf_compression -lpthread -lz -lsnappy
./perf_compression
```

### 获取推荐并发配置
```bash
./build/log-agent --recommend-concurrency
./build/log-router --recommend-concurrency
```

详见 [TEST_CASES.md](tests/TEST_CASES.md)

## 协议规范

### Agent ↔ Router TCP 协议

```
消息格式：
┌─────────────┬─────────┬────────┬────────┬──────────────┐
│ 4B Magic    │ 2B Ver. │ 1B Cmp │ 3B Rsv │ 4B Length    │
│ 0xCAFEBABE  │ 0x0100  │ Type   │        │ Payload Size │
└─────────────┴─────────┴────────┴────────┴──────────────┘
┌──────────────┬──────────────┬──────────────────────────┐
│ 8B Task ID   │ 8B Offset    │ N B Payload              │
│              │              │ (Optional Compressed)    │
└──────────────┴──────────────┴──────────────────────────┘

消息类型（Message Type）：
  0x01: DATA       - 日志数据消息
  0x02: ACK        - 确认消息
  0x03: HEARTBEAT  - 心跳消息
  0x04: METRICS    - 监控指标消息

压缩类型（Compression Type）：
  0x00: NONE   - 无压缩
  0x01: ZLIB   - Zlib 压缩
  0x02: SNAPPY - Snappy 压缩
```

## 监控指标

### Agent 指标
- `agent_logs_collected_total` - 采集总行数
- `agent_bytes_collected_total` - 发送总字节数
- `agent_error_count_total` - 错误总数
- `agent_files_watched` - 监控文件数量

### Router 指标
- `router_active_connections` - 活跃 Agent 连接数
- `router_mq_messages_sent_total` - 消息队列发送成功总数
- `router_mq_messages_failed_total` - 消息队列发送失败总数
- `router_compression_ratio` - 平均压缩比

访问 Prometheus 指标：`http://router-ip:9101/metrics`

## 性能优化建议

### 压缩算法选择
- **无压缩**: 适合本地网络，优先级最低的场景
- **Snappy**: 平衡压缩率和速度，推荐用于一般场景
- **Zlib**: 最高压缩率，适合带宽受限场景

### 并发配置
基于 CPU 核心数和内存大小：

```bash
# 小型部署（1-2 GB 内存，2-4 核）
server.io_threads = 4
backpressure.session_buffer_high_watermark_mb = 32

# 中型部署（4-8 GB 内存，4-8 核）
server.io_threads = 8
backpressure.session_buffer_high_watermark_mb = 64

# 大型部署（16+ GB 内存，8+ 核）
server.io_threads = 16
backpressure.session_buffer_high_watermark_mb = 128
```

## 故障排查

### Agent 无法连接到 Router
- 检查网络连接：`telnet router-host 9090`
- 检查防火墙规则
- 查看 Agent 日志

### Router 消息队列连接失败
- 验证消息队列服务是否运行
- 检查 brokers 配置是否正确
- 验证网络连接

### 高内存占用
- 降低 `session_buffer_high_watermark_mb` 配置
- 增加 Router 实例数量
- 使用更高效的压缩算法

## 贡献指南

1. 创建新的特性分支：`git checkout -b feature/xxx`
2. 提交更改：`git commit -am 'Add xxx'`
3. 推送到远程：`git push origin feature/xxx`
4. 创建 Pull Request

## 设计文档

详见 [DESIGN.md](DESIGN.md)

## 许可证

MIT License

## 联系方式

问题和建议请提交到项目 Issue。
