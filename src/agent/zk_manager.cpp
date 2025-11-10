#include "agent/zk_manager.h"
#include "agent/task_manager.h"
#include <zookeeper/zookeeper.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <thread>
#include <chrono>
#include <sys/utsname.h>

using json = nlohmann::json;

namespace logpipeline {

class ZKManager::Impl {
public:
    Impl(const std::vector<std::string>& hosts) : hosts_(hosts), zh_(nullptr) {
        std::string conn_string;
        for (size_t i = 0; i < hosts_.size(); ++i) {
            if (i > 0) conn_string += ",";
            conn_string += hosts_[i];
        }
        
        zoo_set_debug_level(ZOO_LOG_LEVEL_ERROR);
        zh_ = zookeeper_init(conn_string.c_str(), nullptr, 30000, nullptr, nullptr, 0);
    }
    
    ~Impl() {
        if (zh_) {
            zookeeper_close(zh_);
        }
    }
    
    bool is_connected() {
        if (!zh_) return false;
        return zoo_state(zh_) == ZOO_CONNECTED_STATE;
    }
    
    bool create_node(const std::string& path, const std::string& value, bool ephemeral = false) {
        if (!is_connected()) return false;
        
        int flags = ephemeral ? ZOO_EPHEMERAL : 0;
        char path_buffer[1024];
        return zoo_acreate2(zh_, path.c_str(), value.c_str(), value.length(),
                         &ZOO_OPEN_ACL_UNSAFE, flags, path_buffer, sizeof(path_buffer), 
                         [](int rc, const char *value, const void *data) {
                             // Completion callback
                         }, nullptr) == ZOK;
    }
    
    bool exists(const std::string& path) {
        if (!is_connected()) return false;
        struct Stat stat;
        return zoo_exists(zh_, path.c_str(), 0, &stat) == ZOK;
    }
    
    bool get_data(const std::string& path, std::string& data) {
        if (!is_connected()) return false;
        
        char buffer[4096];
        int buffer_len = sizeof(buffer);
        struct Stat stat;
        if (zoo_get(zh_, path.c_str(), 0, buffer, &buffer_len, &stat) == ZOK) {
            data.assign(buffer, buffer_len);
            return true;
        }
        return false;
    }
    
    bool watch_data(const std::string& path, watcher_fn watcher, void* watcherCtx) {
        if (!is_connected()) return false;
        
        char buffer[4096];
        int buffer_len = sizeof(buffer);
        struct Stat stat;
        return zoo_wget(zh_, path.c_str(), watcher, watcherCtx, buffer, &buffer_len, &stat) == ZOK;
    }
    
    bool ensure_path(const std::string& path) {
        if (!is_connected()) return false;
        
        std::string current;
        std::istringstream ss(path);
        std::string segment;
        
        while (std::getline(ss, segment, '/')) {
            if (segment.empty()) continue;
            
            current += "/" + segment;
            if (!exists(current)) {
                char path_buffer[1024];
                if (zoo_create2(zh_, current.c_str(), nullptr, 0,
                              &ZOO_OPEN_ACL_UNSAFE, 0, path_buffer, sizeof(path_buffer), nullptr) != ZOK) {
                    return false;
                }
            }
        }
        return true;
    }
    
private:
    std::vector<std::string> hosts_;
    zhandle_t* zh_;
};

ZKManager::ZKManager(const std::vector<std::string>& hosts,
                     const std::string& agent_status_path,
                     const std::string& task_config_path,
                     const std::string& agent_id)
    : hosts_(hosts), agent_status_path_(agent_status_path), 
      task_config_path_(task_config_path), agent_id_(agent_id),
      task_manager_(nullptr), running_(false) {
    impl_ = std::make_unique<Impl>(hosts_);
}

ZKManager::~ZKManager() {
    stop();
}

bool ZKManager::start() {
    if (running_) {
        return true;
    }
    
    if (!impl_->is_connected()) {
        spdlog::error("Failed to connect to ZooKeeper");
        return false;
    }
    
    // Ensure paths exist
    if (!impl_->ensure_path(agent_status_path_) || !impl_->ensure_path(task_config_path_)) {
        spdlog::error("Failed to ensure ZooKeeper paths exist");
        return false;
    }
    
    register_agent();
    
    running_ = true;
    watcher_thread_ = std::thread(&ZKManager::watch_task_config, this);
    
    spdlog::info("ZKManager started for agent: {}", agent_id_);
    return true;
}

void ZKManager::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    if (watcher_thread_.joinable()) {
        watcher_thread_.join();
    }
    
    spdlog::info("ZKManager stopped");
}

void ZKManager::set_task_manager(TaskManager* task_manager) {
    task_manager_ = task_manager;
}

void ZKManager::register_agent() {
    struct utsname hostname;
    uname(&hostname);
    
    json agent_info = {
        {"agent_id", agent_id_},
        {"hostname", hostname.nodename},
        {"ip", "127.0.0.1"}, // Simplified - should get actual IP
        {"port", 0},
        {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()},
        {"status", "active"}
    };
    
    std::string agent_path = agent_status_path_ + "/" + agent_id_;
    std::string agent_data = agent_info.dump();
    
    if (impl_->create_node(agent_path, agent_data, true)) {
        spdlog::info("Registered agent in ZooKeeper: {}", agent_path);
    } else {
        spdlog::error("Failed to register agent in ZooKeeper");
    }
}

void ZKManager::watch_task_config() {
    auto watcher_callback = [](zhandle_t* zh, int type, int state, const char* path, void* watcherCtx) {
        if (type == ZOO_CHANGED_EVENT) {
            ZKManager* manager = static_cast<ZKManager*>(watcherCtx);
            manager->on_task_config_changed(std::string(path));
        }
    };
    
    while (running_) {
        if (impl_->is_connected()) {
            std::string data;
            if (impl_->watch_data(task_config_path_, watcher_callback, this)) {
                if (impl_->get_data(task_config_path_, data)) {
                    on_task_config_changed(task_config_path_);
                }
            }
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

void ZKManager::on_task_config_changed(const std::string& path) {
    std::string data;
    if (!impl_->get_data(path, data)) {
        spdlog::error("Failed to get task config from ZooKeeper");
        return;
    }
    
    try {
        auto tasks = parse_task_config(data);
        if (task_manager_) {
            task_manager_->update_tasks(tasks);
        }
        spdlog::info("Updated {} tasks from ZooKeeper", tasks.size());
    } catch (const std::exception& e) {
        spdlog::error("Failed to parse task config: {}", e.what());
    }
}

std::vector<TaskConfig> ZKManager::parse_task_config(const std::string& json_data) {
    std::vector<TaskConfig> tasks;
    
    try {
        json j = json::parse(json_data);
        
        if (j.contains("tasks") && j["tasks"].is_array()) {
            for (const auto& task_json : j["tasks"]) {
                TaskConfig config;
                config.task_id = task_json.value("task_id", "");
                config.log_path_pattern = task_json.value("log_path_pattern", "");
                config.kafka_topic = task_json.value("kafka_topic", "");
                
                if (task_json.contains("properties") && task_json["properties"].is_object()) {
                    for (const auto& [key, value] : task_json["properties"].items()) {
                        config.properties[key] = value.get<std::string>();
                    }
                }
                
                if (!config.task_id.empty() && !config.log_path_pattern.empty()) {
                    tasks.push_back(config);
                }
            }
        }
    } catch (const json::exception& e) {
        throw std::runtime_error("JSON parsing error: " + std::string(e.what()));
    }
    
    return tasks;
}

} // namespace logpipeline
