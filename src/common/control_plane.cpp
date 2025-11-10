#include "common/control_plane.h"
#include <spdlog/spdlog.h>
#include <sstream>

namespace logpipeline {

ControlPlaneClient::ControlPlaneClient(const ControlPlaneConfig& config)
    : config_(config) {
}

ControlPlaneClient::~ControlPlaneClient() {
}

bool ControlPlaneClient::initialize() {
    spdlog::info("Initializing ControlPlaneClient: {}:{}", config_.host, config_.port);
    // 可以在这里进行连接测试
    return true;
}

bool ControlPlaneClient::register_agent(const AgentRegistration& registration,
                                       AgentRegistrationResponse& response) {
    spdlog::info("Registering agent: {}", registration.agent_name);
    
    json request_body;
    request_body["agent_name"] = registration.agent_name;
    request_body["host_name"] = registration.host_name;
    request_body["local_ip"] = registration.local_ip;
    request_body["pid"] = registration.pid;
    request_body["version"] = registration.version;
    
    int http_status = 0;
    std::string response_str = make_http_request("POST", "/api/v1/agents/register",
                                                 request_body, http_status);
    
    if (http_status != 200) {
        response.success = false;
        response.error_msg = "HTTP " + std::to_string(http_status);
        spdlog::error("Agent registration failed: {}", response.error_msg);
        return false;
    }
    
    try {
        auto resp = json::parse(response_str);
        response.success = resp.value("success", false);
        response.agent_id = resp.value("agent_id", "");
        response.error_msg = resp.value("error_msg", "");
        
        if (response.success) {
            spdlog::info("Agent registered successfully with ID: {}", response.agent_id);
        } else {
            spdlog::error("Agent registration error: {}", response.error_msg);
        }
        return response.success;
    } catch (const std::exception& e) {
        spdlog::error("Failed to parse agent registration response: {}", e.what());
        return false;
    }
}

bool ControlPlaneClient::get_task_config(const std::string& agent_id,
                                        int current_cversion,
                                        ConfigVersion& config) {
    spdlog::debug("Getting task config for agent: {}, cversion: {}", 
                  agent_id, current_cversion);
    
    json request_body;
    request_body["agent_id"] = agent_id;
    request_body["cversion"] = current_cversion;
    
    int http_status = 0;
    std::string response_str = make_http_request("POST", "/api/v1/tasks/config",
                                                 request_body, http_status);
    
    if (http_status != 200) {
        spdlog::error("Failed to get task config: HTTP {}", http_status);
        return false;
    }
    
    try {
        auto resp = json::parse(response_str);
        config.cversion = resp.value("cversion", 0);
        
        if (resp.contains("tasks") && resp["tasks"].is_array()) {
            for (const auto& task_json : resp["tasks"]) {
                TaskConfig task;
                task.task_id = task_json.value("task_id", 0);
                task.task_name = task_json.value("task_name", "");
                task.log_path_pattern = task_json.value("log_path_pattern", "");
                task.version = task_json.value("version", 0);
                
                if (task_json.contains("message_queue")) {
                    const auto& mq = task_json["message_queue"];
                    task.message_queue.type = mq.value("type", "kafka");
                    task.message_queue.version = mq.value("version", "");
                    task.message_queue.cluster_id = mq.value("cluster_id", "");
                    task.message_queue.topic = mq.value("topic", "");
                    task.message_queue.properties = mq.value("properties", json::object());
                }
                
                task.properties = task_json.value("properties", json::object());
                config.tasks.push_back(task);
            }
        }
        
        spdlog::info("Got task config with cversion: {}, {} tasks", 
                    config.cversion, config.tasks.size());
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to parse task config response: {}", e.what());
        return false;
    }
}

bool ControlPlaneClient::register_router(const std::string& router_id,
                                        const std::string& host,
                                        int port) {
    spdlog::info("Registering router: {} at {}:{}", router_id, host, port);
    
    json request_body;
    request_body["router_id"] = router_id;
    request_body["host"] = host;
    request_body["port"] = port;
    
    int http_status = 0;
    std::string response_str = make_http_request("POST", "/api/v1/routers/register",
                                                 request_body, http_status);
    
    if (http_status != 200) {
        spdlog::error("Router registration failed: HTTP {}", http_status);
        return false;
    }
    
    spdlog::info("Router registered successfully");
    return true;
}

bool ControlPlaneClient::report_router_status(const std::string& router_id,
                                             const json& status) {
    spdlog::debug("Reporting router status for: {}", router_id);
    
    json request_body;
    request_body["router_id"] = router_id;
    request_body["status"] = status;
    
    int http_status = 0;
    make_http_request("POST", "/api/v1/routers/status",
                      request_body, http_status);
    
    return http_status == 200;
}

std::string ControlPlaneClient::make_http_request(const std::string& method,
                                                  const std::string& path,
                                                  const json& body,
                                                  int& http_status) {
    // 这是一个占位符实现，实际应该使用 libcurl 或其他 HTTP 库
    // 这里简单地记录请求并返回模拟响应
    spdlog::debug("HTTP {} {}", method, path);
    
    // TODO: 实现实际的 HTTP 请求
    // 暂时返回一个模拟的成功响应
    http_status = 200;
    return "{}";
}

} // namespace logpipeline
