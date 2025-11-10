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

class MetricsAggregator {
public:
    MetricsAggregator();
    ~MetricsAggregator();
    
    void update_agent_metrics(const std::string& agent_id,
                             const std::string& metrics_json);
    
    void increment_kafka_messages_sent(const std::string& topic);
    void increment_kafka_messages_failed(const std::string& topic);
    void set_active_connections(int count);
    
    std::string get_metrics_text() const;
    
private:
#ifdef HAVE_PROMETHEUS_CPP
    std::shared_ptr<prometheus::Registry> registry_;
    
    // Forwarder metrics
    std::shared_ptr<prometheus::Gauge> active_connections_gauge_;
    std::shared_ptr<prometheus::Counter> kafka_messages_sent_counter_;
    std::shared_ptr<prometheus::Counter> kafka_messages_failed_counter_;
    
    // Agent metrics
    std::shared_ptr<prometheus::Counter> agent_lines_collected_counter_;
    std::shared_ptr<prometheus::Counter> agent_bytes_collected_counter_;
    std::shared_ptr<prometheus::Counter> agent_error_count_counter_;
    std::shared_ptr<prometheus::Gauge> agent_files_watched_gauge_;
    
    mutable std::mutex metrics_mutex_;
#endif
    
    // Fallback storage when prometheus-cpp is not available
    struct AgentMetrics {
        uint64_t lines_collected{0};
        uint64_t bytes_collected{0};
        uint64_t error_count{0};
        int fast_files{0};
        int slow_files{0};
    };
    
    std::unordered_map<std::string, AgentMetrics> agent_metrics_;
    mutable std::mutex fallback_mutex_;
    
    void parse_and_update_metrics(const std::string& agent_id,
                                 const std::string& metrics_json);
    std::string generate_fallback_metrics() const;
};

} // namespace logpipeline
