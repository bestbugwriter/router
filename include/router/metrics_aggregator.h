#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>

#ifdef HAVE_PROMETHEUS_CPP
#include <prometheus/registry.h>
#include <prometheus/gauge.h>
#include <prometheus/counter.h>
#include <prometheus/family.h>
#endif

namespace logpipeline {

// Router 指标聚合器
// 负责收集和聚合：
// 1. Router 自身的运行指标
// 2. Agent 上报的监控指标
// 3. 消息队列发送统计
class RouterMetricsAggregator {
public:
    RouterMetricsAggregator();
    ~RouterMetricsAggregator();
    
    // 更新 Agent 指标
    void update_agent_metrics(const std::string& agent_id,
                             const std::string& metrics_json);
    
    // 记录消息队列相关指标
    void increment_mq_messages_sent(const std::string& cluster_id,
                                   const std::string& topic);
    void increment_mq_messages_failed(const std::string& cluster_id,
                                     const std::string& topic);
    
    // 设置活跃连接数
    void set_active_connections(int count);
    
    // 获取 Prometheus 格式的指标
    std::string get_metrics_text() const;
    
    // 获取 JSON 格式的指标（用于上报到 Monitor 服务）
    std::string get_metrics_json() const;

private:
#ifdef HAVE_PROMETHEUS_CPP
    std::shared_ptr<prometheus::Registry> registry_;
    
    // Router 指标
    std::shared_ptr<prometheus::Gauge> active_connections_gauge_;
    std::shared_ptr<prometheus::Counter> mq_messages_sent_counter_;
    std::shared_ptr<prometheus::Counter> mq_messages_failed_counter_;
    
    // Agent 指标
    std::shared_ptr<prometheus::Counter> agent_lines_collected_counter_;
    std::shared_ptr<prometheus::Counter> agent_bytes_collected_counter_;
    std::shared_ptr<prometheus::Counter> agent_error_count_counter_;
    std::shared_ptr<prometheus::Gauge> agent_files_watched_gauge_;
    
    mutable std::mutex metrics_mutex_;
#endif
    
    // 回退存储（当 prometheus-cpp 不可用时）
    struct AgentMetrics {
        uint64_t lines_collected{0};
        uint64_t bytes_collected{0};
        uint64_t error_count{0};
        int files_watched{0};
    };
    
    struct MQMetrics {
        uint64_t messages_sent{0};
        uint64_t messages_failed{0};
    };
    
    std::unordered_map<std::string, AgentMetrics> agent_metrics_;
    std::unordered_map<std::string, MQMetrics> mq_metrics_;
    mutable std::mutex fallback_mutex_;
    
    void parse_and_update_metrics(const std::string& agent_id,
                                 const std::string& metrics_json);
    std::string generate_fallback_metrics() const;
};

} // namespace logpipeline
