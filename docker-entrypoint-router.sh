#!/bin/sh

# Router 容器进入点脚本
# 处理配置文件和环境变量

CONFIG_FILE="${1:-/etc/log-pipeline/router.properties}"
TEMPLATE_FILE="/etc/log-pipeline/router.properties.template"

# 如果配置文件不存在但模板存在，从模板生成
if [ ! -f "$CONFIG_FILE" ] && [ -f "$TEMPLATE_FILE" ]; then
    echo "Generating config from template..."
    cp "$TEMPLATE_FILE" "$CONFIG_FILE"
    
    # 处理环境变量覆盖
    
    if [ ! -z "$ROUTER_ID" ]; then
        sed -i "s/^router.id = .*/router.id = $ROUTER_ID/" "$CONFIG_FILE"
    fi
    
    if [ ! -z "$LISTEN_HOST" ]; then
        sed -i "s/^server.listen_host = .*/server.listen_host = $LISTEN_HOST/" "$CONFIG_FILE"
    fi
    
    if [ ! -z "$LISTEN_PORT" ]; then
        sed -i "s/^server.listen_port = .*/server.listen_port = $LISTEN_PORT/" "$CONFIG_FILE"
    fi
    
    if [ ! -z "$IO_THREADS" ]; then
        sed -i "s/^server.io_threads = .*/server.io_threads = $IO_THREADS/" "$CONFIG_FILE"
    fi
    
    if [ ! -z "$CONTROL_PLATFORM_HOST" ]; then
        sed -i "s|^control.platform.host = .*|control.platform.host = $CONTROL_PLATFORM_HOST|" "$CONFIG_FILE"
    fi
    
    if [ ! -z "$CONTROL_PLATFORM_PORT" ]; then
        sed -i "s/^control.platform.port = .*/control.platform.port = $CONTROL_PLATFORM_PORT/" "$CONFIG_FILE"
    fi
    
    if [ ! -z "$KAFKA_BROKERS" ]; then
        sed -i "s|^mq.kafka.cluster.kafka-cluster-1.brokers = .*|mq.kafka.cluster.kafka-cluster-1.brokers = $KAFKA_BROKERS|" "$CONFIG_FILE"
    fi
    
    if [ ! -z "$MONITOR_HOST" ]; then
        sed -i "s|^monitor.host = .*|monitor.host = $MONITOR_HOST|" "$CONFIG_FILE"
    fi
    
    if [ ! -z "$MONITOR_PORT" ]; then
        sed -i "s/^monitor.port = .*/monitor.port = $MONITOR_PORT/" "$CONFIG_FILE"
    fi
fi

# 启动 Router
exec "$@"
