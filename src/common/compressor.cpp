#include "common/compressor.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cstring>

// 条件编译：如果可用则使用 zlib 和 snappy
#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

#ifdef HAVE_SNAPPY
#include <snappy.h>
#endif

namespace logpipeline {
namespace compression {

// NoCompressor 实现
bool NoCompressor::compress(const std::vector<uint8_t>& input,
                           std::vector<uint8_t>& output) {
    output = input;
    return true;
}

bool NoCompressor::decompress(const std::vector<uint8_t>& input,
                             std::vector<uint8_t>& output) {
    output = input;
    return true;
}

// ZlibCompressor 实现
ZlibCompressor::ZlibCompressor(int compression_level)
    : compression_level_(compression_level) {
    // 限制压缩级别在 1-9 之间
    compression_level_ = std::max(1, std::min(9, compression_level_));
}

bool ZlibCompressor::compress(const std::vector<uint8_t>& input,
                             std::vector<uint8_t>& output) {
#ifdef HAVE_ZLIB
    if (input.empty()) {
        output.clear();
        return true;
    }
    
    // 预分配输出缓冲区（通常压缩后会更小）
    unsigned long compressed_size = compressBound(input.size());
    output.resize(compressed_size);
    
    int ret = compress2(output.data(), &compressed_size,
                       input.data(), input.size(),
                       compression_level_);
    
    if (ret != Z_OK) {
        spdlog::error("Zlib compression failed with code: {}", ret);
        return false;
    }
    
    output.resize(compressed_size);
    return true;
#else
    spdlog::error("Zlib compression not available");
    return false;
#endif
}

bool ZlibCompressor::decompress(const std::vector<uint8_t>& input,
                               std::vector<uint8_t>& output) {
#ifdef HAVE_ZLIB
    if (input.empty()) {
        output.clear();
        return true;
    }
    
    // 需要知道原始大小，这里假设不超过 1MB
    unsigned long decompressed_size = 1024 * 1024;
    output.resize(decompressed_size);
    
    int ret = uncompress(output.data(), &decompressed_size,
                        input.data(), input.size());
    
    if (ret != Z_OK) {
        spdlog::error("Zlib decompression failed with code: {}", ret);
        return false;
    }
    
    output.resize(decompressed_size);
    return true;
#else
    spdlog::error("Zlib decompression not available");
    return false;
#endif
}

// SnappyCompressor 实现
bool SnappyCompressor::compress(const std::vector<uint8_t>& input,
                               std::vector<uint8_t>& output) {
#ifdef HAVE_SNAPPY
    if (input.empty()) {
        output.clear();
        return true;
    }
    
    size_t compressed_size = snappy::MaxCompressedLength(input.size());
    output.resize(compressed_size);
    
    snappy::RawCompress(reinterpret_cast<const char*>(input.data()),
                       input.size(),
                       reinterpret_cast<char*>(output.data()),
                       &compressed_size);
    
    output.resize(compressed_size);
    return true;
#else
    spdlog::error("Snappy compression not available");
    return false;
#endif
}

bool SnappyCompressor::decompress(const std::vector<uint8_t>& input,
                                 std::vector<uint8_t>& output) {
#ifdef HAVE_SNAPPY
    if (input.empty()) {
        output.clear();
        return true;
    }
    
    size_t uncompressed_size = 0;
    if (!snappy::GetUncompressedLength(reinterpret_cast<const char*>(input.data()),
                                      input.size(),
                                      &uncompressed_size)) {
        spdlog::error("Failed to get snappy uncompressed length");
        return false;
    }
    
    output.resize(uncompressed_size);
    
    if (!snappy::RawUncompress(reinterpret_cast<const char*>(input.data()),
                              input.size(),
                              reinterpret_cast<char*>(output.data()))) {
        spdlog::error("Snappy decompression failed");
        return false;
    }
    
    return true;
#else
    spdlog::error("Snappy decompression not available");
    return false;
#endif
}

// CompressorFactory 实现
std::unique_ptr<Compressor> CompressorFactory::create(CompressionType type,
                                                     int compression_level) {
    switch (type) {
        case CompressionType::NONE:
            return std::make_unique<NoCompressor>();
        case CompressionType::ZLIB:
            return std::make_unique<ZlibCompressor>(compression_level);
        case CompressionType::SNAPPY:
            return std::make_unique<SnappyCompressor>();
        default:
            spdlog::error("Unknown compression type: {}", static_cast<int>(type));
            return std::make_unique<NoCompressor>();
    }
}

std::unique_ptr<Compressor> CompressorFactory::create_from_string(const std::string& type_str,
                                                                 int compression_level) {
    CompressionType type = parse_type(type_str);
    return create(type, compression_level);
}

CompressionType CompressorFactory::parse_type(const std::string& type_str) {
    std::string lower_type = type_str;
    std::transform(lower_type.begin(), lower_type.end(),
                  lower_type.begin(), ::tolower);
    
    if (lower_type == "none" || lower_type == "off") {
        return CompressionType::NONE;
    } else if (lower_type == "zlib") {
        return CompressionType::ZLIB;
    } else if (lower_type == "snappy") {
        return CompressionType::SNAPPY;
    } else {
        spdlog::warn("Unknown compression type: {}, using NONE", type_str);
        return CompressionType::NONE;
    }
}

} // namespace compression
} // namespace logpipeline
