# 多阶段构建 - 编译阶段
FROM ubuntu:22.04 as builder

# 安装构建依赖
RUN apt-get update && apt-get install -y \
    cmake \
    build-essential \
    git \
    pkg-config \
    libzookeeper-mt-dev \
    librdkafka-dev \
    zlib1g-dev \
    libsnappy-dev \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

# 复制源码
COPY . .

# 构建静态二进制文件
RUN chmod +x build.sh && ./build.sh --static

# 最终阶段 - 运行时镜像
FROM alpine:latest

# 安装运行时依赖
RUN apk add --no-cache \
    ca-certificates \
    tini

# 创建应用用户
RUN addgroup -g 1000 app && \
    adduser -D -u 1000 -G app app

# 从构建阶段复制二进制文件
COPY --from=builder /build/build/log-agent /usr/local/bin/
COPY --from=builder /build/build/log-router /usr/local/bin/
COPY --from=builder /build/build/log-forwarder /usr/local/bin/

# 复制配置文件模板
COPY config/agent.properties /etc/log-pipeline/agent.properties.template
COPY config/router.properties /etc/log-pipeline/router.properties.template

# 创建必要的目录
RUN mkdir -p /var/lib/log-agent /var/lib/log-router && \
    chown -R app:app /var/lib/log-agent /var/lib/log-router

# 使用 app 用户运行
USER app

# 健康检查
HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 \
    CMD [ -f /var/lib/log-agent/agentid ] || [ -f /var/lib/log-router/.running ] || exit 1

# 使用 tini 作为 init 进程
ENTRYPOINT ["/sbin/tini", "--"]

# 默认命令（可被覆盖）
CMD ["log-agent", "/etc/log-pipeline/agent.properties"]

---

# Dockerfile for Agent
FROM alpine:latest as agent

# 安装运行时依赖
RUN apk add --no-cache ca-certificates tini

# 创建应用用户
RUN addgroup -g 1000 app && \
    adduser -D -u 1000 -G app app

# 从构建阶段复制二进制文件
COPY --from=builder /build/build/log-agent /usr/local/bin/

# 复制配置文件模板
COPY config/agent.properties /etc/log-pipeline/agent.properties.template

# 创建必要的目录
RUN mkdir -p /var/lib/log-agent && \
    chown -R app:app /var/lib/log-agent

USER app

# 配置进入点脚本
COPY docker-entrypoint-agent.sh /
RUN chmod +x /docker-entrypoint-agent.sh

ENTRYPOINT ["/sbin/tini", "--", "/docker-entrypoint-agent.sh"]

CMD ["log-agent", "/etc/log-pipeline/agent.properties"]

---

# Dockerfile for Router
FROM alpine:latest as router

# 安装运行时依赖
RUN apk add --no-cache ca-certificates tini

# 创建应用用户
RUN addgroup -g 1000 app && \
    adduser -D -u 1000 -G app app

# 从构建阶段复制二进制文件
COPY --from=builder /build/build/log-router /usr/local/bin/

# 复制配置文件模板
COPY config/router.properties /etc/log-pipeline/router.properties.template

# 创建必要的目录
RUN mkdir -p /var/lib/log-router && \
    chown -R app:app /var/lib/log-router

USER app

EXPOSE 9090 9101

# 配置进入点脚本
COPY docker-entrypoint-router.sh /
RUN chmod +x /docker-entrypoint-router.sh

ENTRYPOINT ["/sbin/tini", "--", "/docker-entrypoint-router.sh"]

CMD ["log-router", "/etc/log-pipeline/router.properties"]

---

# Dockerfile for Test Tools (Python-based)
FROM python:3.11-slim as test-tools

# 安装系统依赖
RUN apt-get update && apt-get install -y \
    curl \
    jq \
    && rm -rf /var/lib/apt/lists/*

# 安装 Python 依赖
RUN pip install --no-cache-dir \
    kafka-python==2.0.2 \
    psutil==5.9.4

# 复制测试工具
COPY tests/tools/ /app/tools/

WORKDIR /app

# 设置日志生成器
RUN chmod +x /app/tools/*.py

ENV PYTHONUNBUFFERED=1

ENTRYPOINT ["/usr/local/bin/python"]
