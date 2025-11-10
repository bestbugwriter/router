#pragma once

#include <string>

namespace logpipeline {

struct Checkpoint {
    uint64_t offset;
    std::string file_path;
    uint64_t inode;
    uint64_t file_size;
    std::string last_modified;
};

} // namespace logpipeline
