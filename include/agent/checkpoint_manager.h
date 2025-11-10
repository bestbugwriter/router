#pragma once

#include <string>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include "common/checkpoint.h"

namespace logpipeline {

class CheckpointManager {
public:
    explicit CheckpointManager(const std::string& checkpoint_file);
    
    bool load();
    bool save();
    
    Checkpoint get_checkpoint(const std::string& key) const;
    void set_checkpoint(const std::string& key, const Checkpoint& checkpoint);
    
    void remove_checkpoint(const std::string& key);
    
    std::unordered_map<std::string, Checkpoint> get_all_checkpoints() const;
    
private:
    std::string checkpoint_file_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, Checkpoint> checkpoints_;
    std::atomic<bool> dirty_{false};
    
    std::string serialize() const;
    bool deserialize(const std::string& data);
    bool atomic_write(const std::string& data);
};

} // namespace logpipeline
