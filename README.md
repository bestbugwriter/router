# C++ 日志采集与转发系统

高性能的C++日志采集（Agent）与转发（Forwarder）系统，支持ZooKeeper动态配置、Kafka消息转发和Prometheus指标监控。

## 架构概览

系统分为两个核心组件：

### 数据采集模块 (Agent)
- 部署在日志源服务器上
- 通过ZooKeeper订阅任务配置并上报状态
- 监控、读取日志文件，进行预处理
- 通过TCP将日志数据和监控指标发送到转发模块

### 数据转发模块 (Forwarder)
- 作为独立服务运行
- 接收来自多个Agent的数据流和指标流
- 将数据流写入Kafka
- 将指标流聚合，并通过HTTP（Prometheus格式）暴露给监控系统

## 技术栈

- **构建系统**: CMake 3.16+
- **网络I/O**: Asio (高性能异步I/O)
- **Kafka客户端**: librdkafka
- **服务发现**: Apache ZooKeeper C客户端
- **JSON处理**: nlohmann/json
- **日志记录**: spdlog
- **配置解析**: inih
- **指标监控**: prometheus-cpp (可选)

## 依赖安装

### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install -y cmake build-essential libzookeeper-mt-dev librdkafka-dev pkg-config
```

### CentOS/RHEL
```bash
sudo yum install -y cmake gcc-c++ make zookeeper-devel librdkafka-devel pkgconfig
```

## 编译

```bash
mkdir build
cd build
cmake ..
make -j$(nproc)
```

## 配置

### Agent配置 (config/agent.properties)
```properties
# Agent Configuration
agent.id = agent-001
agent.forwarders = 127.0.0.1:9090
agent.checkpoint_file_path = /var/lib/log-agent/offsets.json

# ZooKeeper Configuration
zookeeper.hosts = 127.0.0.1:2181
zookeeper.agent_status_path = /log-pipeline/agents
zookeeper.task_config_path = /log-pipeline/tasks/default-group

# Metrics Configuration
metrics.report_interval_sec = 10
```

### Forwarder配置 (config/forwarder.properties)
```properties
# Server Configuration
server.listen_host = 0.0.0.0
server.listen_port = 9090
server.io_threads = 8

# Kafka Configuration
kafka.bootstrap.servers = 127.0.0.1:9092
kafka.default_props.queue.buffering.max.ms = 500
kafka.default_props.compression.codec = snappy

# Backpressure Configuration
backpressure.session_buffer_high_watermark_mb = 64
backpressure.session_buffer_low_watermark_mb = 32

# Metrics Configuration
metrics.prometheus_listen = 0.0.0.0:9101
```

## 运行

### 启动Forwarder
```bash
./log-forwarder config/forwarder.properties
```

### 启动Agent
```bash
./log-agent config/agent.properties
```

## ZooKeeper任务配置

在ZooKeeper中创建任务配置节点：

```bash
# 创建任务配置节点
echo '{
  "tasks": [
    {
      "task_id": "task-001",
      "log_path_pattern": "/var/log/app/*.log",
      "kafka_topic": "app-logs",
      "properties": {
        "encoding": "utf-8",
        "multiline": "true"
      }
    }
  ]
}' | zkCli.sh create /log-pipeline/tasks/default-group -
```

## 协议

### Agent <-> Forwarder TCP协议

```
[ 4B Magic   ] (0xCAFEBABE)
[ 2B Version ] (0x0100)
[ 2B Type    ] (0x01: DATA, 0x02: ACK, 0x03: HEARTBEAT, 0x04: METRICS)
[ 4B Length  ] (Payload的数据长度N)
[ 8B TaskID   ] (DATA/ACK时使用，METRICS时可为0)
[ 8B Offset   ] (DATA/ACK时使用，METRICS时可为0)
[ N B Payload] (日志数据 或 指标JSON)
```

## 监控指标

### Forwarder指标
- `forwarder_active_connections`: 活跃Agent连接数
- `forwarder_kafka_messages_sent_total`: Kafka发送成功总数
- `forwarder_kafka_messages_failed_total`: Kafka发送失败总数

### Agent指标
- `agent_lines_collected_total`: 采集总行数
- `agent_bytes_collected_total`: 发送总字节数
- `agent_error_count_total`: 错误总数
- `agent_files_watched`: 监控文件数量

访问 `http://forwarder-ip:9101/metrics` 查看Prometheus格式的指标。

## 特性

- **高性能**: 基于Asio的异步I/O，支持高并发
- **可靠传输**: TCP协议，支持ACK确认和重传
- **动态配置**: 通过ZooKeeper实现热配置更新
- **断点续传**: 本地点位文件，支持Agent重启后继续采集
- **反压控制**: Forwarder支持高低水位反压控制
- **指标监控**: 完整的Prometheus指标支持
- **跨平台**: 支持Linux、Windows、macOS

## 许可证

MIT License
