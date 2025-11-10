#include "agent/checkpoint_manager.h"
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

using json = nlohmann::json;

namespace logpipeline {

CheckpointManager::CheckpointManager(const std::string& checkpoint_file)
    : checkpoint_file_(checkpoint_file) {
}

bool CheckpointManager::load() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!std::filesystem::exists(checkpoint_file_)) {
        spdlog::info("Checkpoint file does not exist, starting fresh: {}", checkpoint_file_);
        return true;
    }
    
    try {
        std::ifstream file(checkpoint_file_);
        if (!file.is_open()) {
            spdlog::error("Failed to open checkpoint file for reading: {}", checkpoint_file_);
            return false;
        }
        
        std::string data((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
        
        return deserialize(data);
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to load checkpoints: {}", e.what());
        return false;
    }
}

bool CheckpointManager::save() {
    if (!dirty_.load()) {
        return true; // No changes to save
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        std::string data = serialize();
        if (atomic_write(data)) {
            dirty_ = false;
            spdlog::debug("Saved {} checkpoints to {}", checkpoints_.size(), checkpoint_file_);
            return true;
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to save checkpoints: {}", e.what());
    }
    
    return false;
}

Checkpoint CheckpointManager::get_checkpoint(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = checkpoints_.find(key);
    return (it != checkpoints_.end()) ? it->second : Checkpoint{};
}

void CheckpointManager::set_checkpoint(const std::string& key, const Checkpoint& checkpoint) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    checkpoints_[key] = checkpoint;
    dirty_ = true;
}

void CheckpointManager::remove_checkpoint(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (checkpoints_.erase(key) > 0) {
        dirty_ = true;
    }
}

std::unordered_map<std::string, Checkpoint> CheckpointManager::get_all_checkpoints() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return checkpoints_;
}

std::string CheckpointManager::serialize() const {
    json j;
    
    for (const auto& [key, checkpoint] : checkpoints_) {
        json cp;
        cp["offset"] = checkpoint.offset;
        cp["file_path"] = checkpoint.file_path;
        cp["inode"] = checkpoint.inode;
        cp["file_size"] = checkpoint.file_size;
        cp["last_modified"] = checkpoint.last_modified;
        
        j[key] = cp;
    }
    
    return j.dump(2);
}

bool CheckpointManager::deserialize(const std::string& data) {
    try {
        json j = json::parse(data);
        
        checkpoints_.clear();
        
        for (const auto& [key, cp_json] : j.items()) {
            Checkpoint checkpoint;
            checkpoint.offset = cp_json.value("offset", 0);
            checkpoint.file_path = cp_json.value("file_path", "");
            checkpoint.inode = cp_json.value("inode", 0);
            checkpoint.file_size = cp_json.value("file_size", 0);
            checkpoint.last_modified = cp_json.value("last_modified", "");
            
            checkpoints_[key] = checkpoint;
        }
        
        spdlog::info("Loaded {} checkpoints", checkpoints_.size());
        return true;
        
    } catch (const json::exception& e) {
        spdlog::error("JSON parsing error in checkpoint file: {}", e.what());
        return false;
    }
}

bool CheckpointManager::atomic_write(const std::string& data) {
    std::string temp_file = checkpoint_file_ + ".tmp";
    
    try {
        // Write to temporary file
        std::ofstream file(temp_file, std::ios::binary);
        if (!file.is_open()) {
            spdlog::error("Failed to create temporary checkpoint file: {}", temp_file);
            return false;
        }
        
        file.write(data.c_str(), data.size());
        file.close();
        
        // Verify write
        std::ifstream verify(temp_file, std::ios::binary | std::ios::ate);
        if (!verify.is_open() || static_cast<size_t>(verify.tellg()) != data.size()) {
            std::filesystem::remove(temp_file);
            spdlog::error("Checkpoint file write verification failed");
            return false;
        }
        verify.close();
        
        // Atomic rename
        std::error_code ec;
        std::filesystem::rename(temp_file, checkpoint_file_, ec);
        
        if (ec) {
            std::filesystem::remove(temp_file);
            spdlog::error("Failed to rename temporary checkpoint file: {}", ec.message());
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        spdlog::error("Exception during atomic write: {}", e.what());
        std::filesystem::remove(temp_file);
        return false;
    }
}

} // namespace logpipeline
