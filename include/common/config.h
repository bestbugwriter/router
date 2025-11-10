#pragma once

#include <string>
#include <vector>
#include <map>

namespace logpipeline {
namespace config {

class Properties {
public:
    bool load_from_file(const std::string& filename);
    
    std::string get_string(const std::string& key, const std::string& default_value = "") const;
    int get_int(const std::string& key, int default_value = 0) const;
    bool get_bool(const std::string& key, bool default_value = false) const;
    
    std::vector<std::string> get_string_list(const std::string& key, const std::string& delimiter = ",") const;
    
private:
    std::map<std::string, std::string> properties_;
};

struct AgentConfig {
    std::string agent_id;
    std::vector<std::string> forwarders;
    std::string checkpoint_file_path;
    
    // ZooKeeper
    std::vector<std::string> zookeeper_hosts;
    std::string agent_status_path;
    std::string task_config_path;
    
    // Metrics
    int metrics_report_interval_sec;
    
    bool load_from_file(const std::string& filename);
};

struct ForwarderConfig {
    std::string listen_host;
    int listen_port;
    int io_threads;
    
    // Kafka
    std::vector<std::string> kafka_bootstrap_servers;
    std::map<std::string, std::string> kafka_default_props;
    
    // Backpressure
    int session_buffer_high_watermark_mb;
    int session_buffer_low_watermark_mb;
    
    // Metrics
    std::string metrics_prometheus_listen;
    
    // Load shedding
    int load_shedding_max_buffer_full_time_sec;
    
    bool load_from_file(const std::string& filename);
};

// Router 配置（替代 Forwarder）
struct RouterConfig {
    std::string router_id;  // Router 唯一标识
    std::string listen_host;
    int listen_port;
    int io_threads;
    
    // 管控平台
    std::string control_platform_host;
    int control_platform_port;
    
    // 消息队列集群配置
    struct MQClusterConfig {
        std::vector<std::string> brokers;  // Kafka 和 RabbitMQ 使用
        std::string broker_url;  // Pulsar 使用
        std::string version;
        std::map<std::string, std::string> properties;
    };
    
    std::map<std::string, MQClusterConfig> kafka_clusters;
    std::map<std::string, MQClusterConfig> rabbitmq_clusters;
    std::map<std::string, MQClusterConfig> pulsar_clusters;
    
    // 反压
    int session_buffer_high_watermark_mb;
    int session_buffer_low_watermark_mb;
    
    // 指标
    std::string metrics_prometheus_listen;
    
    // 监控服务
    std::string monitor_host;
    int monitor_port;
    int monitor_send_interval_sec;
    
    bool load_from_file(const std::string& filename);
};

} // namespace config
} // namespace logpipeline
