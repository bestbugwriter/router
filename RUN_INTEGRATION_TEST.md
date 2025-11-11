# 集成测试快速指南

本指南说明如何使用新的完全容器化的集成测试。

## 快速开始

```bash
# 进入项目目录
cd /path/to/project

# 运行测试（推荐方式）
./run_integration_test.sh
```

就这么简单！无需安装任何 Python 环境。

## 自定义参数

```bash
# 运行 2 分钟的测试，每秒 500 条日志
TEST_DURATION=120 LOG_RATE=500 ./run_integration_test.sh

# 运行快速测试（30 秒）
TEST_DURATION=30 ./run_integration_test.sh

# 慢速测试（5 分钟，低速率）
TEST_DURATION=300 LOG_RATE=50 ./run_integration_test.sh
```

## 使用 Makefile

```bash
# 最简单的方式
make docker-test

# 查看帮助
make help
```

## 完整的测试过程

脚本会自动执行以下步骤：

1. ✅ **依赖检查** - 检查 Docker 和 docker-compose
2. ✅ **镜像构建** - 构建 Router 镜像（首次会较慢，之后会使用缓存）
3. ✅ **基础服务启动** - 启动 ZooKeeper、Kafka、Control-Platform、Monitor
4. ✅ **Router 启动** - 启动日志路由器服务
5. ✅ **测试执行** - 生成日志、验证数据流
6. ✅ **结果收集** - 收集所有日志和统计数据
7. ✅ **清理资源** - 停止所有容器

## 查看结果

测试完成后，所有结果保存在 `/tmp/integration-test-results-<时间戳>/` 目录：

```bash
# 查看测试报告
cat /tmp/integration-test-results-*/test_report.txt

# 查看 Router 日志
cat /tmp/integration-test-results-*/logs/router.log

# 查看 Kafka 验证结果
cat /tmp/integration-test-results-*/kafka_validator.log

# 查看完整的验证报告（JSON 格式）
cat /tmp/integration-test-results-*/validation_report.json

# 查看系统信息
cat /tmp/integration-test-results-*/system_info.log
```

## 故障排除

### 错误：docker-compose 未找到

```bash
# 检查 docker-compose 是否已安装
docker-compose --version

# 如果未安装，按照官方文档安装：
# https://docs.docker.com/compose/install/
```

### 错误：Cannot connect to Docker daemon

```bash
# 检查 Docker 是否运行
docker ps

# 如果 Docker 未运行，启动它
sudo systemctl start docker

# 或在 macOS 上
open /Applications/Docker.app
```

### 容器启动失败

```bash
# 查看详细的错误信息
docker-compose -f docker-compose.test.yml logs

# 清理旧的容器和镜像
make clean-docker

# 重新运行
./run_integration_test.sh
```

### 测试没有输出

这是正常的。脚本会定期输出进度信息。可以查看实时日志：

```bash
# 在另一个终端窗口中查看结果目录
tail -f /tmp/integration-test-results-*/logs/router.log
```

## 技术细节

### Python 工具容器化

Python 测试工具（日志生成器、Kafka 验证器）已完全容器化，不需要本地 Python 环境。

### Docker Compose 网络

所有服务运行在 `log-pipeline-test` Docker 网络中，确保容器间的通信。

### 端口映射

| 服务 | 容器内部 | 本地访问 |
|------|---------|---------|
| ZooKeeper | 2181 | localhost:2181 |
| Kafka | 29092 (内部), 9092 (外部) | localhost:9092 |
| Control Platform | 80 | localhost:8080 |
| Monitor | 80 | localhost:9200 |
| Router | 9090, 9101 | localhost:9090, localhost:9101 |

## 常见问题

### Q: 为什么第一次运行较慢？

A: 第一次运行需要构建 Docker 镜像。之后会使用缓存，运行速度会快得多。

### Q: 可以同时运行多个测试吗？

A: 可以，但需要使用不同的 Docker Compose 文件或编辑 docker-compose.test.yml 中的端口。

### Q: 如何禁用自动清理？

A: 修改脚本中的 `trap cleanup EXIT` 一行。但通常不需要这样做。

### Q: 测试是否使用真实的 log-agent？

A: 不是。当前测试使用容器化的日志生成器直接生成日志。Agent 支持在后续版本添加。

## 性能提示

### 对于快速反馈

```bash
# 运行 30 秒的快速测试
TEST_DURATION=30 LOG_RATE=100 ./run_integration_test.sh
```

### 对于压力测试

```bash
# 运行 5 分钟的高负载测试
TEST_DURATION=300 LOG_RATE=1000 ./run_integration_test.sh
```

### 对于持久化测试

```bash
# 运行长时间测试
TEST_DURATION=1800 LOG_RATE=500 ./run_integration_test.sh
```

## 更多帮助

查看完整的测试文档：

```bash
cat TESTING.md
cat tests/README.md
```

## 相关命令

```bash
# 构建项目
make build

# 清理构建文件
make clean

# 清理 Docker 资源
make clean-docker

# 显示所有可用命令
make help
```

## 下一步

- [查看架构设计文档](DESIGN.md)
- [查看项目 README](README.md)
- [浏览测试框架](tests/README.md)
