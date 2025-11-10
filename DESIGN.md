# 日志采集与转发系统 - 设计文档

## 1. 系统概述

本系统是一个高性能的C++日志采集与路由系统，支持多种消息队列、网络压缩、分布式配置管理。

### 核心组件
- **Agent**：部署在日志源服务器上，采集日志数据
- **Router**：独立转发服务，接收Agent数据，发送到消息队列或监控系统
- **管控平台**：中央管理服务，管理任务配置、Agent注册、监控
- **Monitor服务**：已有服务，接收监控指标

## 2. 整体架构

```
┌─────────────────────────────────────────────────────────────────┐
│                       管控平台 (Control Plane)                   │
│  ┌──────────────────┐         ┌──────────────────┐             │
│  │ Task Config API  │         │ Agent Registry   │             │
│  │ (HTTP)           │         │ (HTTP)           │             │
│  └──────────────────┘         └──────────────────┘             │
│         ▲                              ▲                        │
└─────────┼──────────────────────────────┼────────────────────────┘
          │                              │
   ┌──────┴────────┐            ┌───────┴──────────┐
   │ Pull Config   │            │ Register/Monitor │
   │ cversion      │            │                  │
   │ Get agentId   │            │                  │
   └───────────────┘            └──────────────────┘
          │                              │
          ▼                              ▼
┌─────────────────┐                ┌──────────────────┐
│    Agent        │                │  ZooKeeper       │
│  ┌──────────────┐────────────────│  (Registration   │
│  │ Config       │  ZK Watch      │   & Monitor)     │
│  │ Manager      │  cversion      │                  │
│  │ (config file)│◄───────────────│  Watch Tasks     │
│  └──────────────┘  Changed       │                  │
│  ┌──────────────┐                │                  │
│  │ File Tailer  │                └──────────────────┘
│  └──────────────┘
│  ┌──────────────┐        ┌──────────────────┐
│  │ TCP Client   │────────│     Router       │
│  │ (compress)   │ TCP    │  ┌──────────────┐│
│  └──────────────┘        │  │Decompress    ││
│  ┌──────────────┐        │  │Multiple MQ   ││
│  │ Metrics      │        │  │(Kafka/Rabbit││
│  │ Collector    │        │  │ /Pulsar)     ││
│  └──────────────┘        │  │              ││
│                          │  │TCP→Monitor   ││
└─────────────────────────────│  (Metrics)   ││
                             │  └──────────────┤│
                             └──────────────────┘
                                      ▼
                            ┌──────────────────┐
                            │ Monitor Service  │
                            │ (TCP, Metrics)   │
                            └──────────────────┘
```

## 3. 详细设计

### 3.1 Agent 启动流程

```
Start Agent
   │
   ├─ Load config from file
   │   └─ Fallback to env variables
   │
   ├─ Check agentId file
   │   ├─ If NOT exists (First Start)
   │   │  └─ Register with Control Plane
   │   │     └─ Get agentId + Save to file
   │   │
   │   └─ If exists (Restart)
   │      └─ Load agentId from file
   │
   ├─ Pull task config from Control Plane
   │   └─ params: agentId, cversion (0 if first time)
   │
   ├─ Start components:
   │   ├─ Checkpoint Manager
   │   ├─ TCP Client (to Router)
   │   ├─ Metrics Collector
   │   ├─ Task Manager
   │   └─ ZK Manager
   │      └─ Register agentId + status
   │      └─ Watch cversion changes
   │
   ├─ Main Loop:
   │   ├─ Monitor config version
   │   │  └─ If cversion changes → pull new config
   │   ├─ Process logs via tailers
   │   ├─ Send data to Router (with compression)
   │   └─ Save checkpoints periodically
   │
   └─ Graceful shutdown
```

### 3.2 Router 架构（原Forwarder）

Router 不依赖 ZooKeeper，只通过管控平台交互：

```
Router Startup:
  1. Load config from file
  2. Connect to all configured Message Queue clusters
  3. Register self to Control Plane (host:port)
  4. Start TCP Server to accept Agent connections
  5. Start Monitor client to send metrics

Data Flow:
  Agent(compressed) → Router(decompress) → MQ Producer → Message Queue
  
  Agent metrics → Router aggregation → Monitor Service
```

### 3.3 消息队列支持

支持的消息队列：
- **Kafka**: 0.8, 0.11, 1.x, 2.x, 3.x
- **RabbitMQ**: 3.x, 4.x  
- **Apache Pulsar**: 2.x, 3.x

#### 生产者抽象层

```cpp
// 基础接口
class MessageQueueProducer {
public:
    virtual bool send(const Message& msg) = 0;
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual uint64_t get_sent_count() = 0;
    virtual uint64_t get_failed_count() = 0;
};

// 实现：KafkaProducer, RabbitMQProducer, PulsarProducer
```

任务配置中指定消息队列类型和版本：

```json
{
  "task_id": "task-001",
  "log_path_pattern": "/var/log/app/*.log",
  "message_queue": {
    "type": "kafka",      // kafka, rabbitmq, pulsar
    "version": "3.x",
    "cluster_id": "kafka-cluster-1",  // 标识不同的集群
    "topic": "app-logs"
  }
}
```

### 3.4 网络传输压缩

修改协议支持压缩：

```
协议头增加压缩字段：
[ 4B Magic   ] (0xCAFEBABE)
[ 2B Version ] (0x0100)
[ 2B Type    ] (0x01-0x04)
[ 1B Compress] (0x00: None, 0x01: zlib, 0x02: snappy)
[ 3B Reserved]
[ 4B Length  ] (压缩后数据长度)
[ 8B TaskID  ]
[ 8B Offset  ]
[ N B Payload] (压缩数据)
```

压缩器接口：

```cpp
class Compressor {
public:
    virtual bool compress(const std::vector<uint8_t>& input,
                         std::vector<uint8_t>& output) = 0;
    virtual bool decompress(const std::vector<uint8_t>& input,
                           std::vector<uint8_t>& output) = 0;
};

// 实现：ZlibCompressor, SnappyCompressor, NoCompressor
```

### 3.5 配置管理

#### 配置优先级

1. 命令行参数（如果支持）
2. 环境变量
3. 配置文件
4. 默认值

#### Agent 配置项

```properties
# 基础配置
agent.id = agent-001  # 如果为空，则注册获取
agent.name = log-agent-1

# 管控平台地址
control.platform.host = 127.0.0.1
control.platform.port = 8080
control.platform.api_version = v1

# 路由服务地址（多个）
router.servers = 127.0.0.1:9090,127.0.0.1:9091

# ZooKeeper（只用于注册和监控）
zookeeper.hosts = 127.0.0.1:2181
zookeeper.agent_status_path = /log-pipeline/agents
zookeeper.watch_path = /log-pipeline/configs

# 本地文件
checkpoint.file_path = /var/lib/log-agent/offsets.json
agent_id.file_path = /var/lib/log-agent/agentid

# 网络传输
network.compression = snappy  # none, zlib, snappy
network.buffer_size_mb = 10

# 指标报告
metrics.report_interval_sec = 10
```

#### Router 配置项

```properties
# 服务配置
server.listen_host = 0.0.0.0
server.listen_port = 9090
server.io_threads = 8

# 管控平台注册
control.platform.host = 127.0.0.1
control.platform.port = 8080
control.platform.register_interval = 30

# 消息队列群集配置
# Kafka集群1
mq.kafka.cluster.kafka-cluster-1.brokers = kafka1:9092,kafka2:9092
mq.kafka.cluster.kafka-cluster-1.version = 3.x
mq.kafka.cluster.kafka-cluster-1.props.compression.type = snappy

# RabbitMQ集群
mq.rabbitmq.cluster.rabbit-cluster-1.hosts = rabbit1:5672
mq.rabbitmq.cluster.rabbit-cluster-1.version = 4.x

# Pulsar集群
mq.pulsar.cluster.pulsar-cluster-1.url = pulsar://pulsar1:6650
mq.pulsar.cluster.pulsar-cluster-1.version = 3.x

# Monitor服务
monitor.host = 127.0.0.1
monitor.port = 9200
monitor.send_interval_sec = 10

# 反压控制
backpressure.session_buffer_high_watermark_mb = 64
backpressure.session_buffer_low_watermark_mb = 32
```

### 3.6 版本管理 (cversion)

任务配置包含版本号，用于增量更新：

```json
{
  "cversion": 1,  // 配置版本号
  "tasks": [
    {
      "task_id": "task-001",
      "version": 1,  // 任务版本
      ...
    }
  ]
}
```

Agent 通过 ZooKeeper watch 监听 cversion 变化，变化时从管控平台重新拉取最新配置。

## 4. 编译配置

### 静态编译

```bash
# 编译选项
-DCMAKE_BUILD_TYPE=Release
-DBUILD_SHARED_LIBS=OFF  # 静态库
-DCMAKE_EXE_LINKER_FLAGS="-static"  # 静态链接
```

支持的编译目标：
- Linux (支持 glibc, musl)
- 静态二进制文件

## 5. Docker 部署

### Agent Docker 镜像
```dockerfile
FROM alpine:latest
RUN apk add --no-cache ca-certificates
COPY log-agent /usr/local/bin/
COPY config/agent.properties /etc/log-pipeline/
ENTRYPOINT ["log-agent"]
```

### Router Docker 镜像
```dockerfile
FROM alpine:latest
RUN apk add --no-cache ca-certificates
COPY log-router /usr/local/bin/
COPY config/router.properties /etc/log-pipeline/
ENTRYPOINT ["log-router"]
```

## 6. 测试策略

### 单元测试
- Mock 外部服务（ZooKeeper, MQ, HTTP）
- 协议编解码测试
- 配置解析测试
- 压缩解压测试

### 性能测试
- 吞吐量测试：不同并发下的消息吞吐
- 延迟测试：端到端延迟
- 内存使用：不同配置下的内存占用
- CPU使用：压缩开销

### 自动化测试
- 集成测试：Agent + Router + MQ
- 故障恢复测试
- 配置热更新测试

## 7. 监控指标

### Agent 指标
- `agent_logs_collected_total`: 采集总行数
- `agent_bytes_collected_total`: 发送总字节数
- `agent_error_count_total`: 错误总数
- `agent_files_watched`: 监控文件数量
- `agent_buffer_usage_bytes`: 缓冲区使用

### Router 指标
- `router_active_connections`: 活跃Agent连接数
- `router_mq_messages_sent_total`: 消息队列发送成功总数
- `router_mq_messages_failed_total`: 消息队列发送失败总数
- `router_compression_ratio`: 平均压缩比
- `router_buffer_usage_bytes`: 缓冲区使用

## 8. 设计模式

1. **工厂模式**：MessageQueueProducer 的创建
2. **策略模式**：Compressor 的选择
3. **观察者模式**：ZooKeeper 配置变化通知
4. **单例模式**：全局配置
5. **模板方法模式**：基础组件的启动/停止流程

