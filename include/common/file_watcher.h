#pragma once

#include <string>
#include <functional>
#include <memory>
#include <vector>

namespace logpipeline {

enum class FileEventType {
    CREATED,
    MODIFIED,
    DELETED,
    MOVED
};

struct FileEvent {
    std::string path;
    FileEventType type;
    size_t size;
};

class IFileWatcher {
public:
    virtual ~IFileWatcher() = default;
    
    virtual bool start_watching(const std::string& path, bool recursive = true) = 0;
    virtual void stop_watching(const std::string& path) = 0;
    
    virtual void set_event_callback(std::function<void(const FileEvent&)> callback) = 0;
    
    virtual bool is_watching(const std::string& path) const = 0;
};

class FileWatcherFactory {
public:
    static std::unique_ptr<IFileWatcher> create();
};

} // namespace logpipeline
