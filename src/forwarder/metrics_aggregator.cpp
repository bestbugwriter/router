#include "forwarder/metrics_aggregator.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <sstream>

using json = nlohmann::json;

namespace logpipeline {

MetricsAggregator::MetricsAggregator() {
#ifdef HAVE_PROMETHEUS_CPP
    registry_ = std::make_shared<prometheus::Registry>();
    
    // Create forwarder metrics
    auto& active_connections_family = prometheus::BuildGauge()
        .Name("forwarder_active_connections")
        .Help("Number of active agent connections")
        .Register(*registry_);
    active_connections_gauge_ = active_connections_family.Add({});
    
    auto& kafka_sent_family = prometheus::BuildCounter()
        .Name("forwarder_kafka_messages_sent_total")
        .Help("Total number of messages sent to Kafka")
        .Register(*registry_);
    kafka_messages_sent_counter_ = &kafka_sent_family;
    
    auto& kafka_failed_family = prometheus::BuildCounter()
        .Name("forwarder_kafka_messages_failed_total")
        .Help("Total number of messages failed to send to Kafka")
        .Register(*registry_);
    kafka_messages_failed_counter_ = &kafka_failed_family;
    
    // Create agent metrics
    auto& lines_family = prometheus::BuildCounter()
        .Name("agent_lines_collected_total")
        .Help("Total number of lines collected by agents")
        .Register(*registry_);
    agent_lines_collected_counter_ = &lines_family;
    
    auto& bytes_family = prometheus::BuildCounter()
        .Name("agent_bytes_collected_total")
        .Help("Total number of bytes collected by agents")
        .Register(*registry_);
    agent_bytes_collected_counter_ = &bytes_family;
    
    auto& errors_family = prometheus::BuildCounter()
        .Name("agent_error_count_total")
        .Help("Total number of errors encountered by agents")
        .Register(*registry_);
    agent_error_count_counter_ = &errors_family;
    
    auto& files_family = prometheus::BuildGauge()
        .Name("agent_files_watched")
        .Help("Number of files being watched by agents")
        .Register(*registry_);
    agent_files_watched_gauge_ = &files_family;
#endif
}

MetricsAggregator::~MetricsAggregator() {
}

void MetricsAggregator::update_agent_metrics(const std::string& agent_id,
                                            const std::string& metrics_json) {
#ifdef HAVE_PROMETHEUS_CPP
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    parse_and_update_metrics(agent_id, metrics_json);
#else
    std::lock_guard<std::mutex> lock(fallback_mutex_);
    parse_and_update_metrics(agent_id, metrics_json);
#endif
}

void MetricsAggregator::increment_kafka_messages_sent(const std::string& topic) {
#ifdef HAVE_PROMETHEUS_CPP
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    if (kafka_messages_sent_counter_) {
        kafka_messages_sent_counter_->Add({{"topic", topic}});
    }
#endif
}

void MetricsAggregator::increment_kafka_messages_failed(const std::string& topic) {
#ifdef HAVE_PROMETHEUS_CPP
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    if (kafka_messages_failed_counter_) {
        kafka_messages_failed_counter_->Add({{"topic", topic}});
    }
#endif
}

void MetricsAggregator::set_active_connections(int count) {
#ifdef HAVE_PROMETHEUS_CPP
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    if (active_connections_gauge_) {
        active_connections_gauge_->Set(count);
    }
#endif
}

std::string MetricsAggregator::get_metrics_text() const {
#ifdef HAVE_PROMETHEUS_CPP
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    if (registry_) {
        std::ostringstream oss;
        // Use prometheus-cpp to generate text format
        // This is simplified - in reality you'd use the TextSerializer
        for (const auto& family : registry_->Collect()) {
            oss << family.name << "\n";
            for (const auto& metric : family.metric) {
                oss << metric.name;
                if (!metric.label.empty()) {
                    oss << "{";
                    bool first = true;
                    for (const auto& label : metric.label) {
                        if (!first) oss << ",";
                        oss << label.name << "=\"" << label.value << "\"";
                        first = false;
                    }
                    oss << "}";
                }
                oss << " " << metric.value << "\n";
            }
        }
        return oss.str();
    }
#endif
    
    return generate_fallback_metrics();
}

void MetricsAggregator::parse_and_update_metrics(const std::string& agent_id,
                                                const std::string& metrics_json) {
    try {
        json j = json::parse(metrics_json);
        
        if (j.contains("tasks") && j["tasks"].is_array()) {
            for (const auto& task : j["tasks"]) {
                std::string task_id = task.value("task_id", "");
                
#ifdef HAVE_PROMETHEUS_CPP
                // Update prometheus metrics
                if (agent_lines_collected_counter_ && task.contains("lines_collected")) {
                    agent_lines_collected_counter_->Add(
                        {{"agent_id", agent_id}, {"task_id", task_id}}
                    ).Increment(task["lines_collected"].get<uint64_t>());
                }
                
                if (agent_bytes_collected_counter_ && task.contains("bytes_collected")) {
                    agent_bytes_collected_counter_->Add(
                        {{"agent_id", agent_id}, {"task_id", task_id}}
                    ).Increment(task["bytes_collected"].get<uint64_t>());
                }
                
                if (agent_error_count_counter_ && task.contains("error_count")) {
                    agent_error_count_counter_->Add(
                        {{"agent_id", agent_id}, {"task_id", task_id}}
                    ).Increment(task["error_count"].get<uint64_t>());
                }
                
                if (agent_files_watched_gauge_) {
                    if (task.contains("fast_files")) {
                        agent_files_watched_gauge_->Add(
                            {{"agent_id", agent_id}, {"task_id", task_id}, {"type", "fast"}}
                        ).Set(task["fast_files"].get<int>());
                    }
                    if (task.contains("slow_files")) {
                        agent_files_watched_gauge_->Add(
                            {{"agent_id", agent_id}, {"task_id", task_id}, {"type", "slow"}}
                        ).Set(task["slow_files"].get<int>());
                    }
                }
#else
                // Fallback storage
                AgentMetrics& metrics = agent_metrics_[agent_id + ":" + task_id];
                if (task.contains("lines_collected")) {
                    metrics.lines_collected += task["lines_collected"].get<uint64_t>();
                }
                if (task.contains("bytes_collected")) {
                    metrics.bytes_collected += task["bytes_collected"].get<uint64_t>();
                }
                if (task.contains("error_count")) {
                    metrics.error_count += task["error_count"].get<uint64_t>();
                }
                if (task.contains("fast_files")) {
                    metrics.fast_files = task["fast_files"].get<int>();
                }
                if (task.contains("slow_files")) {
                    metrics.slow_files = task["slow_files"].get<int>();
                }
#endif
            }
        }
        
    } catch (const json::exception& e) {
        spdlog::error("Failed to parse metrics JSON: {}", e.what());
    }
}

std::string MetricsAggregator::generate_fallback_metrics() const {
    std::lock_guard<std::mutex> lock(fallback_mutex_);
    
    std::ostringstream oss;
    
    // Forwarder metrics
    oss << "# HELP forwarder_active_connections Number of active agent connections\n";
    oss << "# TYPE forwarder_active_connections gauge\n";
    oss << "forwarder_active_connections 0\n\n";
    
    oss << "# HELP forwarder_kafka_messages_sent_total Total number of messages sent to Kafka\n";
    oss << "# TYPE forwarder_kafka_messages_sent_total counter\n";
    oss << "forwarder_kafka_messages_sent_total 0\n\n";
    
    // Agent metrics
    oss << "# HELP agent_lines_collected_total Total number of lines collected by agents\n";
    oss << "# TYPE agent_lines_collected_total counter\n";
    
    for (const auto& [key, metrics] : agent_metrics_) {
        oss << "agent_lines_collected_total{agent_id=\"" << key << "\"} " 
            << metrics.lines_collected << "\n";
    }
    
    oss << "\n# HELP agent_bytes_collected_total Total number of bytes collected by agents\n";
    oss << "# TYPE agent_bytes_collected_total counter\n";
    
    for (const auto& [key, metrics] : agent_metrics_) {
        oss << "agent_bytes_collected_total{agent_id=\"" << key << "\"} " 
            << metrics.bytes_collected << "\n";
    }
    
    return oss.str();
}

} // namespace logpipeline
