#include "common/config.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace logpipeline {
namespace config {

bool Properties::load_from_file(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        // Remove leading/trailing whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#' || line[0] == '!') {
            continue;
        }
        
        // Find the separator
        size_t pos = line.find('=');
        if (pos == std::string::npos) {
            continue; // Skip invalid lines
        }
        
        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);
        
        // Trim whitespace from key and value
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        
        properties_[key] = value;
    }
    
    return true;
}

std::string Properties::get_string(const std::string& key, const std::string& default_value) const {
    auto it = properties_.find(key);
    return (it != properties_.end()) ? it->second : default_value;
}

int Properties::get_int(const std::string& key, int default_value) const {
    auto it = properties_.find(key);
    if (it != properties_.end()) {
        try {
            return std::stoi(it->second);
        } catch (const std::exception&) {
            // Fall through to default
        }
    }
    return default_value;
}

bool Properties::get_bool(const std::string& key, bool default_value) const {
    auto it = properties_.find(key);
    if (it != properties_.end()) {
        std::string value = it->second;
        std::transform(value.begin(), value.end(), value.begin(), ::tolower);
        return (value == "true" || value == "yes" || value == "1");
    }
    return default_value;
}

std::vector<std::string> Properties::get_string_list(const std::string& key, const std::string& delimiter) const {
    std::vector<std::string> result;
    auto it = properties_.find(key);
    if (it != properties_.end()) {
        std::stringstream ss(it->second);
        std::string item;
        while (std::getline(ss, item, delimiter[0])) {
            // Trim whitespace
            item.erase(0, item.find_first_not_of(" \t"));
            item.erase(item.find_last_not_of(" \t") + 1);
            if (!item.empty()) {
                result.push_back(item);
            }
        }
    }
    return result;
}

bool AgentConfig::load_from_file(const std::string& filename) {
    Properties props;
    if (!props.load_from_file(filename)) {
        return false;
    }
    
    agent_id = props.get_string("agent.id", "unknown-agent");
    forwarders = props.get_string_list("agent.forwarders", ",");
    checkpoint_file_path = props.get_string("agent.checkpoint_file_path", "/var/lib/agent/offsets.json");
    
    zookeeper_hosts = props.get_string_list("zookeeper.hosts", ",");
    agent_status_path = props.get_string("zookeeper.agent_status_path", "/log-pipeline/agents");
    task_config_path = props.get_string("zookeeper.task_config_path", "/log-pipeline/tasks/default-group");
    
    metrics_report_interval_sec = props.get_int("metrics.report_interval_sec", 10);
    
    return true;
}

bool ForwarderConfig::load_from_file(const std::string& filename) {
    Properties props;
    if (!props.load_from_file(filename)) {
        return false;
    }
    
    listen_host = props.get_string("server.listen_host", "0.0.0.0");
    listen_port = props.get_int("server.listen_port", 9090);
    io_threads = props.get_int("server.io_threads", 8);
    
    kafka_bootstrap_servers = props.get_string_list("kafka.bootstrap.servers", ",");
    
    // Load Kafka default properties
    for (const auto& [key, value] : props.get_all()) {
        if (key.find("kafka.default_props.") == 0) {
            std::string prop_key = key.substr(20); // Remove "kafka.default_props."
            kafka_default_props[prop_key] = value;
        }
    }
    
    session_buffer_high_watermark_mb = props.get_int("backpressure.session_buffer_high_watermark_mb", 64);
    session_buffer_low_watermark_mb = props.get_int("backpressure.session_buffer_low_watermark_mb", 32);
    
    metrics_prometheus_listen = props.get_string("metrics.prometheus_listen", "0.0.0.0:9101");
    
    load_shedding_max_buffer_full_time_sec = props.get_int("load_shedding.max_buffer_full_time_sec", 30);
    
    return true;
}

bool RouterConfig::load_from_file(const std::string& filename) {
    Properties props;
    if (!props.load_from_file(filename)) {
        return false;
    }
    
    router_id = props.get_string("router.id", "router-001");
    listen_host = props.get_string("server.listen_host", "0.0.0.0");
    listen_port = props.get_int("server.listen_port", 9090);
    io_threads = props.get_int("server.io_threads", 8);
    
    // 管控平台配置
    control_platform_host = props.get_string("control.platform.host", "127.0.0.1");
    control_platform_port = props.get_int("control.platform.port", 8080);
    
    // Monitor 服务配置
    monitor_host = props.get_string("monitor.host", "127.0.0.1");
    monitor_port = props.get_int("monitor.port", 9200);
    monitor_send_interval_sec = props.get_int("monitor.send_interval_sec", 10);

    // Kafka 默认压缩
    kafka_enable_compression = props.get_bool("kafka.enable_compression", true);
    
    return true;
}

} // namespace config
} // namespace logpipeline
