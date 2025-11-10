#pragma once

#include <string>
#include <map>
#include <nlohmann/json.hpp>
#include <vector>

namespace logpipeline {

// 单个任务的配置信息
struct TaskInfo {
    std::string task_id;
    std::string log_path;
    std::string topic;
    std::string cluster_id;
};

// 任务配置映射 (task_id -> TaskInfo)
using TaskConfigMap = std::map<std::string, TaskInfo>;

// 从 JSON 加载任务配置
inline TaskConfigMap parse_task_config_from_json(const std::string& json_str) {
    TaskConfigMap map;
    try {
        auto json = nlohmann::json::parse(json_str);
        for (const auto& task_json : json["tasks"]) {
            TaskInfo info;
            info.task_id = task_json.at("task_id").get<std::string>();
            info.log_path = task_json.at("log_path_pattern").get<std::string>();
            info.topic = task_json.at("kafka_topic").get<std::string>();
            info.cluster_id = task_json.at("properties").at("kafka_cluster_id").get<std::string>();
            map[info.task_id] = info;
        }
    } catch (const std::exception& e) {
        // spdlog is not available in a header, handle error in caller
    }
    return map;
}

} // namespace logpipeline
