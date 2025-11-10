#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>

namespace logpipeline {

using json = nlohmann::json;

// 管控平台客户端配置
struct ControlPlaneConfig {
    std::string host = "127.0.0.1";
    int port = 8080;
    std::string api_version = "v1";
    int timeout_sec = 10;
    int max_retries = 3;
    int retry_interval_sec = 2;
};

// Agent 注册请求/响应
struct AgentRegistration {
    std::string agent_name;
    std::string host_name;
    std::string local_ip;
    int pid = 0;
    std::string version;
};

struct AgentRegistrationResponse {
    bool success = false;
    std::string agent_id;
    std::string error_msg;
};

// 任务配置
struct TaskConfig {
    uint64_t task_id = 0;
    std::string task_name;
    std::string log_path_pattern;
    int version = 0;  // 任务版本
    
    // 消息队列配置
    struct {
        std::string type;  // "kafka", "rabbitmq", "pulsar"
        std::string version;
        std::string cluster_id;
        std::string topic;
        json properties;
    } message_queue;
    
    // 其他配置
    json properties;
};

// 配置版本信息
struct ConfigVersion {
    int cversion = 0;  // 配置版本号
    std::vector<TaskConfig> tasks;
};

// 管控平台客户端
class ControlPlaneClient {
public:
    explicit ControlPlaneClient(const ControlPlaneConfig& config);
    ~ControlPlaneClient();
    
    // 初始化客户端
    bool initialize();
    
    // Agent 注册
    bool register_agent(const AgentRegistration& registration,
                       AgentRegistrationResponse& response);
    
    // 获取任务配置
    bool get_task_config(const std::string& agent_id,
                        int current_cversion,
                        ConfigVersion& config);
    
    // Router 注册（上报地址）
    bool register_router(const std::string& router_id,
                        const std::string& host,
                        int port);
    
    // 上报 Router 状态
    bool report_router_status(const std::string& router_id,
                             const json& status);
    
private:
    ControlPlaneConfig config_;
    
    // HTTP 请求辅助方法
    std::string make_http_request(const std::string& method,
                                  const std::string& path,
                                  const json& body,
                                  int& http_status);
};

} // namespace logpipeline
