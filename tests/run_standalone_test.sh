#!/bin/bash

set -e

echo "===== Running Standalone Integration Test ====="

# 切换到脚本所在目录的父目录（项目根目录）
cd "$(dirname "$0")/.."

# 1. 构建项目
echo "--> Building the project..."
./build.sh

# 检查构建产物是否存在
ROUTER_EXEC="./build/log-router"
AGENT_EXEC="./build/log-agent"

if [ ! -f "$ROUTER_EXEC" ]; then
    echo "Error: log-router executable not found!"
    exit 1
fi

if [ ! -f "$AGENT_EXEC" ]; then
    echo "Error: log-agent executable not found!"
    exit 1
fi

echo "--> Build complete."

# 2. 启动服务
echo "--> Starting services in background..."
ROUTER_CONFIG="./config/router.properties.standalone"
AGENT_CONFIG="./config/agent.properties.standalone"

# 启动 router
$ROUTER_EXEC $ROUTER_CONFIG &
ROUTER_PID=$!
echo "log-router started with PID: $ROUTER_PID"

# 启动 agent
$AGENT_EXEC $AGENT_CONFIG &
AGENT_PID=$!
echo "log-agent started with PID: $AGENT_PID"

# 3. 等待服务启动
echo "--> Waiting for services to initialize..."
sleep 5

# 4. 检查服务是否仍在运行
if ! kill -0 $ROUTER_PID 2>/dev/null; then
    echo "Error: log-router process is not running!"
    exit 1
fi

if ! kill -0 $AGENT_PID 2>/dev/null; then
    echo "Error: log-agent process is not running!"
    # 清理 router 进程
    kill $ROUTER_PID
    exit 1
fi

echo "--> Services are running."

# 5. 清理和关闭
echo "--> Shutting down services..."
kill $AGENT_PID
kill $ROUTER_PID

# 等待进程完全退出
wait $AGENT_PID 2>/dev/null || true
wait $ROUTER_PID 2>/dev/null || true

echo "--> Services stopped."
echo ""
echo "===== Standalone Integration Test PASSED! ====="

exit 0
