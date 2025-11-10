#include "router/metrics_aggregator.h"
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace logpipeline {

RouterMetricsAggregator::RouterMetricsAggregator() {
#ifdef HAVE_PROMETHEUS_CPP
    registry_ = std::make_shared<prometheus::Registry>();
    
    // 创建指标
    auto& active_conn_family = prometheus::BuildGauge()
        .Name("router_active_connections")
        .Help("Number of active Agent connections")
        .Register(*registry_);
    active_connections_gauge_ =
        &active_conn_family.Add({});
    
    auto& mq_sent_family = prometheus::BuildCounter()
        .Name("router_mq_messages_sent_total")
        .Help("Total messages sent to message queue")
        .Register(*registry_);
    mq_messages_sent_counter_ =
        &mq_sent_family.Add({});
    
    auto& mq_failed_family = prometheus::BuildCounter()
        .Name("router_mq_messages_failed_total")
        .Help("Total messages failed to send to message queue")
        .Register(*registry_);
    mq_messages_failed_counter_ =
        &mq_failed_family.Add({});
#endif
}

RouterMetricsAggregator::~RouterMetricsAggregator() {
}

void RouterMetricsAggregator::update_agent_metrics(const std::string& agent_id,
                                                  const std::string& metrics_json) {
    parse_and_update_metrics(agent_id, metrics_json);
}

void RouterMetricsAggregator::increment_mq_messages_sent(const std::string& cluster_id,
                                                        const std::string& topic) {
#ifdef HAVE_PROMETHEUS_CPP
    if (mq_messages_sent_counter_) {
        mq_messages_sent_counter_->Increment();
    }
#endif
    
    std::lock_guard<std::mutex> lock(fallback_mutex_);
    mq_metrics_[cluster_id].messages_sent++;
}

void RouterMetricsAggregator::increment_mq_messages_failed(const std::string& cluster_id,
                                                          const std::string& topic) {
#ifdef HAVE_PROMETHEUS_CPP
    if (mq_messages_failed_counter_) {
        mq_messages_failed_counter_->Increment();
    }
#endif
    
    std::lock_guard<std::mutex> lock(fallback_mutex_);
    mq_metrics_[cluster_id].messages_failed++;
}

void RouterMetricsAggregator::set_active_connections(int count) {
#ifdef HAVE_PROMETHEUS_CPP
    if (active_connections_gauge_) {
        active_connections_gauge_->Set(count);
    }
#endif
    
    std::lock_guard<std::mutex> lock(fallback_mutex_);
    // 记录在回退存储中
}

std::string RouterMetricsAggregator::get_metrics_text() const {
#ifdef HAVE_PROMETHEUS_CPP
    if (registry_) {
        auto collected = registry_->Collect();
        prometheus::TextSerializer serializer;
        return serializer.Serialize(collected);
    }
#endif
    
    return generate_fallback_metrics();
}

std::string RouterMetricsAggregator::get_metrics_json() const {
    json result;
    
    {
        std::lock_guard<std::mutex> lock(fallback_mutex_);
        
        // Agent 指标
        json agent_metrics;
        for (const auto& [agent_id, metrics] : agent_metrics_) {
            json agent_data;
            agent_data["agent_id"] = agent_id;
            agent_data["lines_collected"] = metrics.lines_collected;
            agent_data["bytes_collected"] = metrics.bytes_collected;
            agent_data["error_count"] = metrics.error_count;
            agent_data["files_watched"] = metrics.files_watched;
            agent_metrics.push_back(agent_data);
        }
        result["agent_metrics"] = agent_metrics;
        
        // 消息队列指标
        json mq_metrics;
        for (const auto& [cluster_id, metrics] : mq_metrics_) {
            json mq_data;
            mq_data["cluster_id"] = cluster_id;
            mq_data["messages_sent"] = metrics.messages_sent;
            mq_data["messages_failed"] = metrics.messages_failed;
            mq_metrics.push_back(mq_data);
        }
        result["mq_metrics"] = mq_metrics;
    }
    
    return result.dump();
}

void RouterMetricsAggregator::parse_and_update_metrics(const std::string& agent_id,
                                                      const std::string& metrics_json) {
    try {
        auto metrics = json::parse(metrics_json);
        
        std::lock_guard<std::mutex> lock(fallback_mutex_);
        auto& agent_metrics = agent_metrics_[agent_id];
        
        agent_metrics.lines_collected = metrics.value("lines_collected", 0);
        agent_metrics.bytes_collected = metrics.value("bytes_collected", 0);
        agent_metrics.error_count = metrics.value("error_count", 0);
        agent_metrics.files_watched = metrics.value("files_watched", 0);
    } catch (const std::exception& e) {
        spdlog::error("Failed to parse agent metrics: {}", e.what());
    }
}

std::string RouterMetricsAggregator::generate_fallback_metrics() const {
    std::string result;
    
    {
        std::lock_guard<std::mutex> lock(fallback_mutex_);
        
        result += "# HELP router_agent_metrics Agent metrics aggregated by router\n";
        result += "# TYPE router_agent_metrics gauge\n";
        
        for (const auto& [agent_id, metrics] : agent_metrics_) {
            result += "router_agent_lines_collected{agent_id=\"" + agent_id + "\"} " +
                     std::to_string(metrics.lines_collected) + "\n";
            result += "router_agent_bytes_collected{agent_id=\"" + agent_id + "\"} " +
                     std::to_string(metrics.bytes_collected) + "\n";
            result += "router_agent_error_count{agent_id=\"" + agent_id + "\"} " +
                     std::to_string(metrics.error_count) + "\n";
            result += "router_agent_files_watched{agent_id=\"" + agent_id + "\"} " +
                     std::to_string(metrics.files_watched) + "\n";
        }
    }
    
    return result;
}

} // namespace logpipeline
