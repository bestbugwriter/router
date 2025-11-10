# 变更日志

## 版本 1.1.0 - 主要改造版本

### 新增功能

#### 1. Router 系统（替代 Forwarder）
- **新建 Router 模块**（`include/router/`、`src/router/`）
  - `RouterTCPServer` - 接收 Agent 连接，支持多消息队列路由
  - `RouterClientSession` - 处理单个 Agent 连接
  - `RouterMetricsAggregator` - 指标聚合（Agent + Router）
  - `RouterMetricsEndpoint` - Prometheus 指标暴露
  - `RouterConfig` - Router 配置类

- **功能增强**
  - 支持多个消息队列集群
  - 支持不同版本的消息队列
  - 支持网络数据压缩（zlib/snappy）
  - 支持从管控平台拉取配置

#### 2. 多消息队列支持
- **新建 MessageQueue 抽象层**（`include/common/message_queue.h`）
  - `MessageQueueProducer` - 生产者基类
  - `KafkaProducerImpl` - Kafka 生产者（支持 0.8, 0.11, 2.x, 3.x 等）
  - `RabbitMQProducerImpl` - RabbitMQ 生产者（支持 3.x, 4.x）
  - `PulsarProducerImpl` - Pulsar 生产者（支持 2.x, 3.x）
  - `MessageQueueProducerFactory` - 生产者工厂

- **配置支持**
  - 每个消息队列集群可独立配置版本和属性
  - Router 可同时向多个消息队列集群发送数据

#### 3. 网络数据压缩
- **新建 Compressor 抽象层**（`include/common/compressor.h`）
  - `Compressor` - 压缩器基类
  - `NoCompressor` - 无压缩（透传）
  - `ZlibCompressor` - Zlib 压缩（可配置压缩级别）
  - `SnappyCompressor` - Snappy 压缩
  - `CompressorFactory` - 压缩器工厂

- **协议扩展**
  - 消息头添加压缩类型字段（1 字节）
  - Agent 发送前压缩，Router 接收后解压

#### 4. 管控平台集成
- **新建 ControlPlane 客户端**（`include/common/control_plane.h`）
  - Agent 注册 API
  - 任务配置拉取 API
  - Router 注册和状态上报 API
  - 支持配置版本（cversion）管理

- **Agent 启动流程改进**
  - 首次启动检查 agentId 文件
  - 不存在时向管控平台注册获取 agentId
  - 从管控平台拉取任务配置
  - 支持通过 ZooKeeper 监听配置变更

#### 5. ZooKeeper 角色调整
- ZooKeeper 现在仅用于：
  - Agent 状态注册和监控
  - 任务配置变更通知（cversion watch）
- 不再存储任务配置（改由管控平台管理）

#### 6. 静态编译支持
- **CMakeLists.txt 改进**
  - 添加 `BUILD_STATIC` 选项
  - 支持可选的 zlib 和 snappy 压缩库
  - 条件编译支持

- **build.sh 改进**
  - 支持 `--static` 参数
  - 支持 `--debug` 参数
  - 改进的参数处理

#### 7. Docker 容器化
- **多阶段 Dockerfile**
  - Builder 阶段：编译应用
  - Agent 阶段：Alpine 基础镜像
  - Router 阶段：Alpine 基础镜像
  - 静态编译，镜像大小最小

- **docker-compose.yml**
  - 完整的开发环境
  - 包含 ZooKeeper、Kafka 等依赖服务
  - 易于快速启动测试

- **进入点脚本**
  - `docker-entrypoint-agent.sh` - Agent 环境变量处理
  - `docker-entrypoint-router.sh` - Router 环境变量处理

#### 8. 测试框架
- **单元测试**
  - `tests/unit/test_protocol.cpp` - 协议编解码测试
  - `tests/unit/test_compressor.cpp` - 压缩器测试

- **性能测试**
  - `tests/performance/perf_compression.cpp` - 压缩性能基准

- **测试工具**
  - `tests/run_unit_tests.sh` - 运行单元测试
  - `tests/run_all_tests.sh` - 运行所有测试
  - `tests/TEST_CASES.md` - 测试用例清单

#### 9. 文档更新
- **DESIGN.md** - 详细的系统设计文档
  - 系统架构图
  - Agent 启动流程
  - Router 架构
  - 消息队列支持
  - 网络传输压缩
  - 配置管理
  - 测试策略
  - 设计模式应用

- **README.md** - 更新所有新特性
  - 架构概览
  - 快速开始指南
  - Docker 部署
  - 测试方法
  - 协议规范
  - 监控指标

### 配置文件变更

#### 新增配置文件
- `config/router.properties` - Router 配置示例

#### 配置项扩展

**Agent 配置新增**：
- `control.platform.host` - 管控平台地址
- `control.platform.port` - 管控平台端口
- `network.compression` - 网络传输压缩算法
- `network.buffer_size_mb` - 网络缓冲区大小

**Router 配置**：
- `router.id` - Router 唯一标识
- `control.platform.host/port` - 管控平台地址
- `mq.kafka.cluster.*` - Kafka 集群配置
- `mq.rabbitmq.cluster.*` - RabbitMQ 集群配置
- `mq.pulsar.cluster.*` - Pulsar 集群配置
- `monitor.host/port` - Monitor 服务地址

### 代码结构改进

#### 新增目录
- `include/router/` - Router 头文件
- `src/router/` - Router 实现
- `tests/unit/` - 单元测试
- `tests/performance/` - 性能测试
- `tests/integration/` - 集成测试（框架）

#### 新增文件统计
- 头文件：8 个（compressor、message_queue、control_plane、router 相关）
- 源文件：8 个（实现文件）
- 测试文件：3 个（单元 + 性能测试）
- 配置文件：3 个（router.properties、docker-entrypoint 脚本）
- 文档文件：2 个（DESIGN.md、CHANGELOG.md）

### 向后兼容性

- `log-forwarder` 可执行文件保留（同时构建为 Router）
- 原有的 forwarder 目录和文件保留
- Agent 和原有配置格式保持兼容

### 设计模式应用

1. **工厂模式**
   - `MessageQueueProducerFactory`
   - `CompressorFactory`

2. **策略模式**
   - `Compressor` 接口及其实现

3. **单例模式**
   - 全局配置对象

4. **模板方法模式**
   - 组件启动/停止框架

5. **观察者模式**
   - ZooKeeper 监听机制

### 性能优化

- 网络传输压缩支持（Zlib/Snappy）
- 可配置的 IO 线程数
- 反压控制（缓冲区高低水位）
- 消息队列集群级别的生产者管理

### 已知局限

- HTTP 客户端实现（ControlPlaneClient）为占位符
- RabbitMQ 和 Pulsar 生产者实现为框架
- 部分集成测试未实现

### 下一步改进

1. 完成 HTTP 客户端实现
2. 完成 RabbitMQ 和 Pulsar 生产者实现
3. 添加完整的集成测试
4. 实现 Agent 配置热更新逻辑
5. 性能优化和调优

### 版本说明

- 兼容版本：1.0.x Agent 和 Forwarder
- 推荐升级版本号：1.1.0
- 主要改进：支持多 MQ、网络压缩、分布式配置
