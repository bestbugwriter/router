#include "agent/file_tailer.h"
#include "agent/checkpoint_manager.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <spdlog/spdlog.h>

namespace logpipeline {

FileTailer::FileTailer(const std::string& task_id,
                       const std::string& file_path,
                       std::function<void(const std::string&, uint64_t, const std::string&)> data_callback)
    : task_id_(task_id), file_path_(file_path), data_callback_(data_callback), running_(false) {
    checkpoint_.offset = 0;
    checkpoint_.file_path = file_path;
    checkpoint_.inode = 0;
    checkpoint_.file_size = 0;
    checkpoint_.last_modified = "";
}

FileTailer::~FileTailer() {
    stop();
}

void FileTailer::start() {
    if (running_) {
        return;
    }
    
    running_ = true;
    tail_thread_ = std::thread(&FileTailer::tail_loop, this);
    
    spdlog::info("Started file tailer for task: {}, file: {}", task_id_, file_path_);
}

void FileTailer::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    if (tail_thread_.joinable()) {
        tail_thread_.join();
    }
    
    spdlog::info("Stopped file tailer for task: {}, file: {}", task_id_, file_path_);
}

Checkpoint FileTailer::get_checkpoint() const {
    return checkpoint_;
}

void FileTailer::set_checkpoint(const Checkpoint& checkpoint) {
    checkpoint_ = checkpoint;
}

void FileTailer::tail_loop() {
    while (running_) {
        try {
            // Check if file exists
            if (!std::filesystem::exists(file_path_)) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }
            
            // Get current file info
            uint64_t current_size = get_current_file_size();
            
            // Check if file was rotated
            if (file_rotated()) {
                // File was rotated, reset offset to 0
                checkpoint_.offset = 0;
                checkpoint_.file_size = current_size;
                spdlog::info("File rotated, resetting offset for: {}", file_path_);
            }
            
            // Read new data
            if (current_size > checkpoint_.offset) {
                std::string data;
                if (read_from_offset(checkpoint_.offset, data)) {
                    if (!data.empty()) {
                        data_callback_(task_id_, checkpoint_.offset, data);
                        update_checkpoint(checkpoint_.offset + data.length());
                        
                        lines_collected_ += std::count(data.begin(), data.end(), '\n');
                        bytes_collected_ += data.length();
                    }
                }
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
        } catch (const std::exception& e) {
            spdlog::error("Error in file tailer for {}: {}", file_path_, e.what());
            error_count_++;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

bool FileTailer::read_from_offset(uint64_t offset, std::string& data) {
    try {
        std::ifstream file(file_path_, std::ios::binary);
        if (!file.is_open()) {
            return false;
        }
        
        file.seekg(offset);
        if (file.fail()) {
            return false;
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        data = buffer.str();
        
        return true;
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to read from file {}: {}", file_path_, e.what());
        return false;
    }
}

uint64_t FileTailer::get_current_file_size() const {
    try {
        return std::filesystem::file_size(file_path_);
    } catch (const std::exception&) {
        return 0;
    }
}

bool FileTailer::file_rotated() const {
    try {
        auto current_size = get_current_file_size();
        auto current_time = std::filesystem::last_write_time(file_path_);
        
        // Simple rotation detection: if current size is less than stored size
        // or the modification time changed significantly
        return current_size < checkpoint_.file_size;
        
    } catch (const std::exception&) {
        return false;
    }
}

void FileTailer::update_checkpoint(uint64_t offset) {
    checkpoint_.offset = offset;
    checkpoint_.file_size = get_current_file_size();
    
    try {
        auto ftime = std::filesystem::last_write_time(file_path_);
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - std::filesystem::file_time_type::clock::now()
            + std::chrono::system_clock::now());
        checkpoint_.last_modified = std::to_string(
            std::chrono::duration_cast<std::chrono::seconds>(sctp.time_since_epoch()).count()
        );
    } catch (const std::exception&) {
        checkpoint_.last_modified = "";
    }
}

} // namespace logpipeline
