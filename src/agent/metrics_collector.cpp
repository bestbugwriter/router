#include "agent/metrics_collector.h"
#include "agent/tcp_client.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <thread>
#include <spdlog/spdlog.h>

using json = nlohmann::json;

namespace logpipeline {

MetricsCollector::MetricsCollector(AgentTCPClient* tcp_client)
    : tcp_client_(tcp_client), running_(false), interval_sec_(10) {
}

MetricsCollector::~MetricsCollector() {
    stop();
}

void MetricsCollector::start(int interval_sec) {
    if (running_) {
        return;
    }
    
    interval_sec_ = interval_sec;
    running_ = true;
    collector_thread_ = std::thread(&MetricsCollector::collect_loop, this);
    
    spdlog::info("MetricsCollector started with interval: {} seconds", interval_sec_);
}

void MetricsCollector::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    if (collector_thread_.joinable()) {
        collector_thread_.join();
    }
    
    spdlog::info("MetricsCollector stopped");
}

void MetricsCollector::increment_lines_collected(const std::string& task_id, uint64_t count) {
    std::lock_guard<std::mutex> lock(mutex_);
    metrics_[task_id].lines_collected += count;
}

void MetricsCollector::increment_bytes_collected(const std::string& task_id, uint64_t bytes) {
    std::lock_guard<std::mutex> lock(mutex_);
    metrics_[task_id].bytes_collected += bytes;
}

void MetricsCollector::increment_error_count(const std::string& task_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    metrics_[task_id].error_count++;
}

void MetricsCollector::set_files_watched(const std::string& task_id, int fast_files, int slow_files) {
    std::lock_guard<std::mutex> lock(mutex_);
    metrics_[task_id].fast_files = fast_files;
    metrics_[task_id].slow_files = slow_files;
}

void MetricsCollector::collect_loop() {
    while (running_) {
        try {
            send_metrics();
            std::this_thread::sleep_for(std::chrono::seconds(interval_sec_));
        } catch (const std::exception& e) {
            spdlog::error("Error in metrics collection loop: {}", e.what());
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

std::string MetricsCollector::serialize_metrics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    json j;
    j["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    json tasks = json::array();
    
    for (const auto& [task_id, metrics] : metrics_) {
        json task;
        task["task_id"] = task_id;
        task["lines_collected"] = metrics.lines_collected;
        task["bytes_collected"] = metrics.bytes_collected;
        task["error_count"] = metrics.error_count;
        task["fast_files"] = metrics.fast_files;
        task["slow_files"] = metrics.slow_files;
        
        tasks.push_back(task);
    }
    
    j["tasks"] = tasks;
    
    return j.dump();
}

void MetricsCollector::send_metrics() {
    if (!tcp_client_) {
        return;
    }
    
    std::string metrics_json = serialize_metrics();
    
    if (tcp_client_->send_metrics(metrics_json)) {
        spdlog::debug("Sent metrics: {}", metrics_json);
        
        // Reset counters after successful send
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [task_id, metrics] : metrics_) {
            // Keep file counts, reset counters
            metrics.lines_collected = 0;
            metrics.bytes_collected = 0;
            metrics.error_count = 0;
        }
    } else {
        spdlog::error("Failed to send metrics");
    }
}

} // namespace logpipeline
