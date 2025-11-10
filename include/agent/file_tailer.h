#pragma once

#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <functional>
#include "common/checkpoint.h"

namespace logpipeline {

class FileTailer {
public:
    FileTailer(const std::string& task_id,
               const std::string& file_path,
               std::function<void(const std::string&, uint64_t, const std::string&)> data_callback);
    
    ~FileTailer();
    
    void start();
    void stop();
    
    Checkpoint get_checkpoint() const;
    void set_checkpoint(const Checkpoint& checkpoint);
    
    uint64_t get_lines_collected() const { return lines_collected_.load(); }
    uint64_t get_bytes_collected() const { return bytes_collected_.load(); }
    uint64_t get_error_count() const { return error_count_.load(); }
    
private:
    std::string task_id_;
    std::string file_path_;
    std::function<void(const std::string&, uint64_t, const std::string&)> data_callback_;
    
    std::atomic<bool> running_;
    std::thread tail_thread_;
    
    std::atomic<uint64_t> lines_collected_{0};
    std::atomic<uint64_t> bytes_collected_{0};
    std::atomic<uint64_t> error_count_{0};
    
    Checkpoint checkpoint_;
    
    void tail_loop();
    bool read_from_offset(uint64_t offset, std::string& data);
    uint64_t get_current_file_size() const;
    bool file_rotated() const;
    void update_checkpoint(uint64_t offset);
};

} // namespace logpipeline
