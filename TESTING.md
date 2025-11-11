# 测试指南

本项目提供了完整的测试框架，支持单元测试、集成测试和性能测试。最新版本完全容器化，无需在本地安装 Python 依赖。

## 快速开始

### 最简单的方法（推荐）

运行完全容器化的集成测试，**无需本地安装任何 Python 环境**：

```bash
# 一键运行测试
make docker-test

# 或直接运行
./run_integration_test.sh
```

这个命令会：
1. ✅ 自动检查 Docker 和 docker-compose
2. ✅ 构建 Router 镜像
3. ✅ 启动 ZooKeeper、Kafka、Control-Platform、Monitor
4. ✅ 启动 Router 服务
5. ✅ 运行容器化的日志生成器
6. ✅ 运行容器化的 Kafka 数据验证
7. ✅ 收集所有日志和结果

## 环境要求

仅需要以下工具：

- **Docker** (version 20.10+)
- **Docker Compose** (version 1.29+)

**不需要**：
- ❌ Python 3.11
- ❌ kafka-python 库
- ❌ 任何其他系统依赖

## 使用方法

### 1. 完全容器化的测试（推荐）

```bash
# 一键运行集成测试
make docker-test

# 或使用脚本直接运行
./run_integration_test.sh

# 自定义参数
TEST_DURATION=120 LOG_RATE=500 ./run_integration_test.sh
```

**参数说明**：
- `TEST_DURATION`: 测试持续时间，单位秒（默认：60）
- `LOG_RATE`: 日志生成速率，单位：条/秒（默认：200）

**示例**：

```bash
# 运行 2 分钟的测试，每秒 500 条日志
TEST_DURATION=120 LOG_RATE=500 ./run_integration_test.sh

# 运行快速测试（30 秒，低速率）
TEST_DURATION=30 LOG_RATE=100 ./run_integration_test.sh
```

### 2. 本地编译后运行测试

如果你本地有 Python 环境，也可以：

```bash
# 编译项目
make build

# 运行测试
make test
```

### 3. 快速测试

```bash
# 运行单元 + 集成测试
make test-quick
```

## 测试结果

运行完成后，结果会保存在 `/tmp/integration-test-results-<TIMESTAMP>/` 目录中：

```
results/
├── logs/
│   ├── router.log          # Router 日志
│   ├── kafka.log           # Kafka 日志
│   └── zookeeper.log       # ZooKeeper 日志
├── log_generator.log       # 日志生成器输出
├── kafka_validator.log     # Kafka 验证器输出
├── system_info.log         # 系统信息
└── test_report.txt         # 测试报告
```

查看结果：

```bash
# 查看 Router 日志
cat /tmp/integration-test-results-*/logs/router.log

# 查看完整报告
cat /tmp/integration-test-results-*/test_report.txt

# 清理结果
rm -rf /tmp/integration-test-results-*
```

## 故障排除

### Docker 镜像构建失败

```bash
# 检查 Docker 是否运行
docker ps

# 清理旧的镜像和容器
make clean-docker

# 重新运行
make docker-test
```

### 容器无法启动

```bash
# 查看详细日志
docker-compose -f docker-compose.test.yml logs router
docker-compose -f docker-compose.test.yml logs kafka
docker-compose -f docker-compose.test.yml logs zookeeper
```

### Kafka 连接失败

```bash
# 检查 Kafka 是否就绪
docker-compose -f docker-compose.test.yml exec kafka kafka-topics --list --bootstrap-server localhost:9092

# 重启所有服务
make clean-docker
make docker-test
```

## 测试架构

### 容器化工具

Python 测试工具现在已完全容器化：

1. **log_generator** - 日志生成工具
   - 生成 JSON/文本格式日志
   - 支持可配置的生成速率
   - 运行在 `python:3.11-slim` 容器中

2. **kafka_validator** - 数据验证工具
   - 消费 Kafka 消息
   - 验证数据完整性
   - 分析性能指标
   - 生成验证报告

### 服务容器

- **zookeeper** - 服务注册和配置管理
- **kafka** - 消息队列
- **control-platform** - 管控平台（httpbin 模拟）
- **monitor** - 监控服务（httpbin 模拟）
- **router** - 日志路由器

## 性能基准

在标准硬件上的性能表现（仅参考）：

| 配置 | 日志速率 | 内存 | CPU | 吞吐量 |
|------|---------|------|-----|--------|
| 低负载 | 100 条/s | ~50MB | <5% | 95条/s |
| 中负载 | 500 条/s | ~150MB | ~15% | 480条/s |
| 高负载 | 1000 条/s | ~300MB | ~30% | 950条/s |

## 常见命令

```bash
# 编译项目
make build                  # 动态链接
make build-static           # 静态链接

# 运行测试
make docker-test            # 容器化集成测试（推荐）
make test-quick             # 快速测试（需要本地 Python）
make test                   # 完整测试（需要本地 Python）

# 清理
make clean                  # 清理构建文件
make clean-docker           # 清理 Docker 资源

# 其他
make help                   # 显示帮助
```

## 开发工作流

### 快速迭代

1. 修改代码
2. 编译项目
   ```bash
   make build
   ```
3. 运行集成测试
   ```bash
   make docker-test
   ```
4. 查看结果
   ```bash
   cat /tmp/integration-test-results-*/test_report.txt
   ```

### 完整测试

提交代码前，运行完整测试：

```bash
make build && make test
```

## 技术细节

### Docker 镜像结构

项目使用多阶段 Dockerfile：

1. **builder** - 编译阶段
   - 安装 CMake、GCC 等构建工具
   - 编译 C++ 代码
   - 输出静态二进制

2. **router** - Router 运行时
   - Alpine Linux + 最小依赖
   - ~50MB 镜像大小

3. **agent** - Agent 运行时
   - Alpine Linux + 最小依赖

4. **test-tools** - 测试工具
   - Python 3.11 环境
   - 预装 kafka-python、psutil

### Docker Compose 网络

所有服务运行在 `log-pipeline-test` 网络中：

```
┌─────────────────────────────────────┐
│    log-pipeline-test (bridge)       │
├─────────────────────────────────────┤
│  ├─ zookeeper:2181                  │
│  ├─ kafka:29092 (container内部)     │
│  ├─ control-platform:80             │
│  ├─ monitor:80                      │
│  └─ router:9090                     │
└─────────────────────────────────────┘
   ↓
外部访问（仅部分服务暴露端口）
   ├─ localhost:2181 (ZooKeeper)
   ├─ localhost:9092 (Kafka)
   ├─ localhost:8080 (Control Platform)
   ├─ localhost:9200 (Monitor)
   └─ localhost:9090 (Router)
```

## 获取帮助

查看项目帮助：

```bash
make help
```

查看测试框架文档：

```bash
cat tests/README.md
```

## 相关文档

- [README.md](README.md) - 项目概述
- [DESIGN.md](DESIGN.md) - 架构设计文档
- [tests/README.md](tests/README.md) - 详细的测试文档
- [CHANGELOG.md](CHANGELOG.md) - 更新日志
