#!/bin/sh

# Agent 容器进入点脚本
# 处理配置文件和环境变量

CONFIG_FILE="${1:-/etc/log-pipeline/agent.properties}"
TEMPLATE_FILE="/etc/log-pipeline/agent.properties.template"

# 如果配置文件不存在但模板存在，从模板生成
if [ ! -f "$CONFIG_FILE" ] && [ -f "$TEMPLATE_FILE" ]; then
    echo "Generating config from template..."
    cp "$TEMPLATE_FILE" "$CONFIG_FILE"
    
    # 处理环境变量覆盖
    # 例如：AGENT_ID=my-agent, ZOOKEEPER_HOSTS=zk1:2181,zk2:2181
    
    if [ ! -z "$AGENT_ID" ]; then
        sed -i "s/^agent.id = .*/agent.id = $AGENT_ID/" "$CONFIG_FILE"
    fi
    
    if [ ! -z "$ROUTER_SERVERS" ]; then
        sed -i "s|^agent.forwarders = .*|agent.forwarders = $ROUTER_SERVERS|" "$CONFIG_FILE"
    fi
    
    if [ ! -z "$ZOOKEEPER_HOSTS" ]; then
        sed -i "s|^zookeeper.hosts = .*|zookeeper.hosts = $ZOOKEEPER_HOSTS|" "$CONFIG_FILE"
    fi
    
    if [ ! -z "$CONTROL_PLATFORM_HOST" ]; then
        sed -i "s|^control.platform.host = .*|control.platform.host = $CONTROL_PLATFORM_HOST|" "$CONFIG_FILE"
    fi
    
    if [ ! -z "$CONTROL_PLATFORM_PORT" ]; then
        sed -i "s/^control.platform.port = .*/control.platform.port = $CONTROL_PLATFORM_PORT/" "$CONFIG_FILE"
    fi
fi

# 启动 Agent
exec "$@"
