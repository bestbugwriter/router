#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <map>

namespace logpipeline {

struct TaskConfig {
    std::string task_id;
    std::string log_path_pattern;
    std::string kafka_topic;
    std::map<std::string, std::string> properties;

    bool operator==(const TaskConfig& other) const {
        return task_id == other.task_id &&
               log_path_pattern == other.log_path_pattern &&
               kafka_topic == other.kafka_topic &&
               properties == other.properties;
    }
};

class AgentTCPClient;
class MetricsCollector;

class TaskManager {
public:
    TaskManager(AgentTCPClient* tcp_client, MetricsCollector* metrics_collector);
    ~TaskManager();
    
    void update_tasks(const std::vector<TaskConfig>& tasks);
    void start();
    void stop();
    
    std::vector<TaskConfig> get_active_tasks() const;
    
private:
    class TaskWorker;
    
    std::unordered_map<std::string, std::unique_ptr<TaskWorker>> workers_;
    bool running_;
    AgentTCPClient* tcp_client_;
    MetricsCollector* metrics_collector_;
    
    void start_worker(const TaskConfig& config);
    void stop_worker(const std::string& task_id);
};

} // namespace logpipeline
