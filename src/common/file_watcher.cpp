#include "common/file_watcher.h"
#include <stdexcept>
#include <algorithm>

#ifdef __linux__
#include <sys/inotify.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <cstring>
#include <map>
#elif defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <CoreServices/CoreServices.h>
#include <sys/stat.h>
#endif

namespace logpipeline {

#ifdef __linux__

class LinuxFileWatcher : public IFileWatcher {
public:
    LinuxFileWatcher() : inotify_fd_(-1) {
        inotify_fd_ = inotify_init1(IN_NONBLOCK);
        if (inotify_fd_ < 0) {
            throw std::runtime_error("Failed to initialize inotify");
        }
    }
    
    ~LinuxFileWatcher() {
        if (inotify_fd_ >= 0) {
            close(inotify_fd_);
        }
    }
    
    bool start_watching(const std::string& path, bool recursive) override {
        uint32_t mask = IN_CREATE | IN_MODIFY | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO;
        int wd = inotify_add_watch(inotify_fd_, path.c_str(), mask);
        if (wd < 0) {
            return false;
        }
        
        watch_descriptors_[wd] = path;
        
        if (recursive) {
            watch_directory_recursive(path);
        }
        
        return true;
    }
    
    void stop_watching(const std::string& path) override {
        auto it = std::find_if(watch_descriptors_.begin(), watch_descriptors_.end(),
                               [&path](const auto& pair) { return pair.second == path; });
        if (it != watch_descriptors_.end()) {
            inotify_rm_watch(inotify_fd_, it->first);
            watch_descriptors_.erase(it);
        }
    }
    
    void set_event_callback(std::function<void(const FileEvent&)> callback) override {
        event_callback_ = callback;
    }
    
    bool is_watching(const std::string& path) const override {
        return std::any_of(watch_descriptors_.begin(), watch_descriptors_.end(),
                          [&path](const auto& pair) { return pair.second == path; });
    }
    
private:
    int inotify_fd_;
    std::map<int, std::string> watch_descriptors_;
    std::function<void(const FileEvent&)> event_callback_;
    
    void watch_directory_recursive(const std::string& dir_path) {
        DIR* dir = opendir(dir_path.c_str());
        if (!dir) return;
        
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            
            std::string full_path = dir_path + "/" + entry->d_name;
            
            struct stat st;
            if (stat(full_path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
                start_watching(full_path, true);
            }
        }
        
        closedir(dir);
    }
};

#elif defined(_WIN32)

class WindowsFileWatcher : public IFileWatcher {
public:
    WindowsFileWatcher() {
        // Implementation for Windows using ReadDirectoryChangesW
        throw std::runtime_error("Windows file watcher not implemented yet");
    }
    
    bool start_watching(const std::string& path, bool recursive) override {
        // Windows implementation
        return false;
    }
    
    void stop_watching(const std::string& path) override {
        // Windows implementation
    }
    
    void set_event_callback(std::function<void(const FileEvent&)> callback) override {
        event_callback_ = callback;
    }
    
    bool is_watching(const std::string& path) const override {
        return false;
    }
    
private:
    std::function<void(const FileEvent&)> event_callback_;
};

#else

class GenericFileWatcher : public IFileWatcher {
public:
    GenericFileWatcher() {
        // Generic implementation using polling
    }
    
    bool start_watching(const std::string& path, bool recursive) override {
        watched_paths_.insert(path);
        return true;
    }
    
    void stop_watching(const std::string& path) override {
        watched_paths_.erase(path);
    }
    
    void set_event_callback(std::function<void(const FileEvent&)> callback) override {
        event_callback_ = callback;
    }
    
    bool is_watching(const std::string& path) const override {
        return watched_paths_.find(path) != watched_paths_.end();
    }
    
private:
    std::set<std::string> watched_paths_;
    std::function<void(const FileEvent&)> event_callback_;
};

#endif

std::unique_ptr<IFileWatcher> FileWatcherFactory::create() {
#ifdef __linux__
    return std::make_unique<LinuxFileWatcher>();
#elif defined(_WIN32)
    return std::make_unique<WindowsFileWatcher>();
#else
    return std::make_unique<GenericFileWatcher>();
#endif
}

} // namespace logpipeline
