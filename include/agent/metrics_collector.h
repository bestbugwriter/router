#pragma once

#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <unordered_map>

namespace logpipeline {

class AgentTCPClient;

class MetricsCollector {
public:
    explicit MetricsCollector(AgentTCPClient* tcp_client);
    ~MetricsCollector();
    
    void start(int interval_sec);
    void stop();
    
    void increment_lines_collected(const std::string& task_id, uint64_t count = 1);
    void increment_bytes_collected(const std::string& task_id, uint64_t bytes);
    void increment_error_count(const std::string& task_id);
    void set_files_watched(const std::string& task_id, int fast_files, int slow_files);
    
private:
    AgentTCPClient* tcp_client_;
    std::atomic<bool> running_;
    std::thread collector_thread_;
    int interval_sec_;
    
    mutable std::mutex mutex_;
    
    struct TaskMetrics {
        uint64_t lines_collected{0};
        uint64_t bytes_collected{0};
        uint64_t error_count{0};
        int fast_files{0};
        int slow_files{0};
    };
    
    std::unordered_map<std::string, TaskMetrics> metrics_;
    
    void collect_loop();
    std::string serialize_metrics() const;
    void send_metrics();
};

} // namespace logpipeline
