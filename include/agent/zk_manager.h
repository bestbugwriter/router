#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>

namespace logpipeline {

struct TaskConfig;
class TaskManager;

class ZKManager {
public:
    ZKManager(const std::vector<std::string>& hosts,
              const std::string& agent_status_path,
              const std::string& task_config_path,
              const std::string& agent_id);
    
    ~ZKManager();
    
    bool start();
    void stop();
    
    void set_task_manager(TaskManager* task_manager);
    
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    
    std::vector<std::string> hosts_;
    std::string agent_status_path_;
    std::string task_config_path_;
    std::string agent_id_;
    
    TaskManager* task_manager_;
    
    std::atomic<bool> running_;
    std::thread watcher_thread_;
    
    void register_agent();
    void watch_task_config();
    void on_task_config_changed(const std::string& data);
    std::vector<TaskConfig> parse_task_config(const std::string& json_data);
};

} // namespace logpipeline
