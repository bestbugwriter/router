#include "agent/task_manager.h"
#include "agent/file_tailer.h"
#include "agent/metrics_collector.h"
#include "agent/tcp_client.h"
#include <algorithm>
#include <set>
#include <spdlog/spdlog.h>

namespace logpipeline {

class TaskManager::TaskWorker {
public:
    TaskWorker(const TaskConfig& config, MetricsCollector* metrics_collector, AgentTCPClient* tcp_client)
        : config_(config), metrics_collector_(metrics_collector), tcp_client_(tcp_client), running_(false) {
    }
    
    ~TaskWorker() {
        stop();
    }
    
    void start() {
        if (running_) {
            return;
        }
        
        running_ = true;
        
        // Create file tailers for matching files
        // For now, we'll create one tailer per task (simplified)
        // In a real implementation, we'd scan for files matching the pattern
        
        auto tailer = std::make_unique<FileTailer>(
            config_.task_id,
            config_.log_path_pattern,
            [this](const std::string& task_id, uint64_t offset, const std::string& data) {
                this->handle_log_data(task_id, offset, data);
            }
        );
        
        tailer->start();
        file_tailers_.push_back(std::move(tailer));
        
        spdlog::info("Started task worker for task: {}", config_.task_id);
    }
    
    void stop() {
        if (!running_) {
            return;
        }
        
        running_ = false;
        
        for (auto& tailer : file_tailers_) {
            tailer->stop();
        }
        file_tailers_.clear();
        
        spdlog::info("Stopped task worker for task: {}", config_.task_id);
    }
    
    const TaskConfig& get_config() const {
        return config_;
    }
    
    uint64_t get_total_lines_collected() const {
        uint64_t total = 0;
        for (const auto& tailer : file_tailers_) {
            total += tailer->get_lines_collected();
        }
        return total;
    }
    
    uint64_t get_total_bytes_collected() const {
        uint64_t total = 0;
        for (const auto& tailer : file_tailers_) {
            total += tailer->get_bytes_collected();
        }
        return total;
    }
    
    int get_fast_file_count() const {
        return static_cast<int>(file_tailers_.size()); // Simplified
    }
    
    int get_slow_file_count() const {
        return 0; // Simplified
    }
    
private:
    TaskConfig config_;
    MetricsCollector* metrics_collector_;
    AgentTCPClient* tcp_client_;
    bool running_;
    std::vector<std::unique_ptr<FileTailer>> file_tailers_;
    
    void handle_log_data(const std::string& task_id, uint64_t offset, const std::string& data) {
        if (!tcp_client_->send_data(std::stoull(task_id), offset, data)) {
            spdlog::error("Failed to send log data for task: {}", task_id);
        }
    }
};

TaskManager::TaskManager(AgentTCPClient* tcp_client, MetricsCollector* metrics_collector) 
    : running_(false), tcp_client_(tcp_client), metrics_collector_(metrics_collector) {
}

TaskManager::~TaskManager() {
    stop();
}

void TaskManager::update_tasks(const std::vector<TaskConfig>& tasks) {
    std::set<std::string> new_task_ids;
    for (const auto& task : tasks) {
        new_task_ids.insert(task.task_id);
    }
    
    // Stop workers for tasks that are no longer needed
    std::vector<std::string> tasks_to_remove;
    for (const auto& [task_id, worker] : workers_) {
        if (new_task_ids.find(task_id) == new_task_ids.end()) {
            tasks_to_remove.push_back(task_id);
        }
    }
    
    for (const auto& task_id : tasks_to_remove) {
        stop_worker(task_id);
    }
    
    // Start or update workers for new tasks
    for (const auto& task : tasks) {
        auto it = workers_.find(task.task_id);
        if (it == workers_.end()) {
            start_worker(task);
        } else {
            // Task exists, check if config changed
            if (!(it->second->get_config() == task)) {
                spdlog::info("Task {} configuration changed, restarting worker.", task.task_id);
                stop_worker(task.task_id);
                start_worker(task);
            }
        }
    }
}

void TaskManager::start() {
    if (running_) {
        return;
    }
    
    running_ = true;
    spdlog::info("TaskManager started");
}

void TaskManager::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    for (auto& [task_id, worker] : workers_) {
        worker->stop();
    }
    workers_.clear();
    
    spdlog::info("TaskManager stopped");
}

std::vector<TaskConfig> TaskManager::get_active_tasks() const {
    std::vector<TaskConfig> tasks;
    for (const auto& [task_id, worker] : workers_) {
        tasks.push_back(worker->get_config());
    }
    return tasks;
}

void TaskManager::start_worker(const TaskConfig& config) {
    auto worker = std::make_unique<TaskWorker>(config, metrics_collector_, tcp_client_);
    worker->start();
    workers_[config.task_id] = std::move(worker);
}

void TaskManager::stop_worker(const std::string& task_id) {
    auto it = workers_.find(task_id);
    if (it != workers_.end()) {
        it->second->stop();
        workers_.erase(it);
    }
}

} // namespace logpipeline
