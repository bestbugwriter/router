# 🚀 快速开始 - 集成测试容器化

## 最简单的方式（推荐）

```bash
make docker-test
```

就这样！无需安装任何 Python 环境。

---

## 发生了什么？

### 问题（之前）
- 需要在本地安装 Python、kafka-python 等依赖
- `docker-compose` 命令在某些系统不可用
- 脚本缺少日志输出，难以追踪进度
- 使用体验不佳

### 解决方案（现在）
✅ **完全容器化** - Python 工具运行在 Docker 中，无需本地依赖
✅ **自动兼容** - 支持 `docker compose` 和 `docker-compose` 两种方式
✅ **详细日志** - 每个步骤都有清晰的进度输出
✅ **一键运行** - 使用 `make docker-test` 或 `./run_integration_test.sh`

---

## 自定义测试参数

```bash
# 2分钟测试，每秒500条日志
TEST_DURATION=120 LOG_RATE=500 make docker-test

# 快速测试（30秒）
TEST_DURATION=30 LOG_RATE=100 make docker-test

# 压力测试（5分钟，每秒1000条）
TEST_DURATION=300 LOG_RATE=1000 make docker-test
```

---

## 可用命令

```bash
# 测试相关
make docker-test       # 运行容器化集成测试（推荐）
make test              # 运行完整测试（需要本地Python）
make test-quick        # 快速测试（需要本地Python）
make test-integration  # 集成测试（需要本地Python）

# 构建相关
make build             # 编译项目
make build-static      # 静态编译
make docker-build      # 构建Docker镜像

# 清理相关
make clean             # 清理构建文件
make clean-docker      # 清理Docker容器
make help              # 显示帮助信息
```

---

## 测试结果在哪里？

测试完成后，查看结果：

```bash
# 查看测试报告
cat /tmp/integration-test-results-*/test_report.txt

# 查看 Router 日志
cat /tmp/integration-test-results-*/logs/router.log

# 查看 Kafka 验证结果
cat /tmp/integration-test-results-*/kafka_validator.log

# 查看Kafka话题详情
cat /tmp/integration-test-results-*/system_info.log
```

---

## 新增的文件

| 文件 | 说明 |
|------|------|
| `run_integration_test.sh` | 主集成测试脚本（容器化） |
| `docker-compose.test.yml` | 测试环境配置 |
| `Makefile` | 便捷命令集合 |
| `TESTING.md` | 详细的测试文档 |
| `RUN_INTEGRATION_TEST.md` | 完整的快速开始指南 |
| `IMPROVEMENTS_SUMMARY.md` | 改进详解 |

---

## 修改的文件

| 文件 | 修改说明 |
|------|----------|
| `Dockerfile` | 添加了 `test-tools` 阶段用于 Python 工具 |
| `tests/integration/run_integration_tests.sh` | 修复路径，添加日志，支持 docker-compose 兼容性 |

---

## 环境要求

- **Docker** (必须)
- **docker-compose** 或 **docker compose** (必须, 新Docker包含)
- ~~Python 3.11~~ ❌ 不需要！
- ~~kafka-python~~ ❌ 不需要！
- ~~psutil~~ ❌ 不需要！

---

## 故障排除

### 问题：`docker-compose` 或 `docker compose` 命令未找到

**解决方案**：
```bash
# 安装 Docker Desktop 或 Docker Engine（包含 docker compose）
# https://docs.docker.com/engine/install/

# 检查安装
docker --version
docker compose version
```

### 问题：容器启动失败

**解决方案**：
```bash
# 清理旧容器
make clean-docker

# 重新运行
make docker-test
```

### 问题：没有看到日志输出

**解决方案**：
- 这是正常的！脚本会定期输出进度
- 查看 `/tmp/integration-test-results-*/` 中的详细日志

---

## 工作流程

```
用户运行: make docker-test
    ↓
脚本检查依赖（Docker, docker-compose）
    ↓
构建 Docker 镜像（第一次较慢）
    ↓
启动 ZooKeeper、Kafka、Control-Platform、Monitor
    ↓
健康检查所有服务
    ↓
启动 Router
    ↓
在 Docker 中运行日志生成器
    ↓
在 Docker 中运行 Kafka 验证器
    ↓
监控进度并输出
    ↓
收集所有日志
    ↓
生成测试报告
    ↓
自动清理容器
    ↓
显示结果位置
```

---

## 更多信息

- 完整文档：[TESTING.md](TESTING.md)
- 详细指南：[RUN_INTEGRATION_TEST.md](RUN_INTEGRATION_TEST.md)
- 改进总结：[IMPROVEMENTS_SUMMARY.md](IMPROVEMENTS_SUMMARY.md)
- 项目首页：[README.md](README.md)

---

## 示例运行

```bash
$ make docker-test

Log Pipeline - 快速命令参考
Log Pipeline 集成测试
=====================================

[INFO] 检查依赖...
[✓] docker 已安装
[✓] docker compose 已安装

[INFO] 构建 Docker 镜像...
[✓] Docker 镜像构建完成

[INFO] 启动基础服务...
[✓] ZooKeeper 就绪
[✓] Kafka 就绪
[✓] 基础服务启动完成

[INFO] 启动 Router...
[✓] Router 就绪

[INFO] 运行日志生成测试...
[✓] 日志生成器已启动
[✓] Kafka 验证器已启动
[✓] 集成测试完成

[INFO] 收集测试结果...
[✓] 测试结果已保存到: /tmp/integration-test-results-20231115_101530

========================================
    集成测试完成！
========================================

✓ 测试结果保存在:
  /tmp/integration-test-results-20231115_101530

快速查看结果:
  cat /tmp/integration-test-results-20231115_101530/test_report.txt
  cat /tmp/integration-test-results-20231115_101530/logs/router.log
  cat /tmp/integration-test-results-20231115_101530/kafka_validator.log
```

---

## 下一步

1. 运行测试：`make docker-test`
2. 检查结果：`cat /tmp/integration-test-results-*/test_report.txt`
3. 浏览文档：`cat TESTING.md`

祝你测试顺利！ 🎉
