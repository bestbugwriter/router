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
};

class TaskManager {
public:
    TaskManager();
    ~TaskManager();
    
    void update_tasks(const std::vector<TaskConfig>& tasks);
    void start();
    void stop();
    
    std::vector<TaskConfig> get_active_tasks() const;
    
private:
    class TaskWorker;
    
    std::unordered_map<std::string, std::unique_ptr<TaskWorker>> workers_;
    bool running_;
    
    void start_worker(const TaskConfig& config);
    void stop_worker(const std::string& task_id);
};

} // namespace logpipeline
