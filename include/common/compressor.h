#pragma once

#include <vector>
#include <memory>
#include <cstdint>
#include <string>

namespace logpipeline {
namespace compression {

// 压缩算法枚举
enum class CompressionType : uint8_t {
    NONE = 0x00,
    ZLIB = 0x01,
    SNAPPY = 0x02
};

// 压缩器基类
class Compressor {
public:
    virtual ~Compressor() = default;
    
    // 压缩数据
    virtual bool compress(const std::vector<uint8_t>& input,
                         std::vector<uint8_t>& output) = 0;
    
    // 解压数据
    virtual bool decompress(const std::vector<uint8_t>& input,
                           std::vector<uint8_t>& output) = 0;
    
    // 获取压缩类型
    virtual CompressionType get_type() const = 0;
};

// 无压缩（透传）
class NoCompressor : public Compressor {
public:
    bool compress(const std::vector<uint8_t>& input,
                 std::vector<uint8_t>& output) override;
    
    bool decompress(const std::vector<uint8_t>& input,
                   std::vector<uint8_t>& output) override;
    
    CompressionType get_type() const override {
        return CompressionType::NONE;
    }
};

// Zlib 压缩
class ZlibCompressor : public Compressor {
public:
    explicit ZlibCompressor(int compression_level = 6);
    
    bool compress(const std::vector<uint8_t>& input,
                 std::vector<uint8_t>& output) override;
    
    bool decompress(const std::vector<uint8_t>& input,
                   std::vector<uint8_t>& output) override;
    
    CompressionType get_type() const override {
        return CompressionType::ZLIB;
    }
    
private:
    int compression_level_;
};

// Snappy 压缩
class SnappyCompressor : public Compressor {
public:
    bool compress(const std::vector<uint8_t>& input,
                 std::vector<uint8_t>& output) override;
    
    bool decompress(const std::vector<uint8_t>& input,
                   std::vector<uint8_t>& output) override;
    
    CompressionType get_type() const override {
        return CompressionType::SNAPPY;
    }
};

// 压缩器工厂
class CompressorFactory {
public:
    static std::unique_ptr<Compressor> create(CompressionType type,
                                              int compression_level = 6);
    
    static std::unique_ptr<Compressor> create_from_string(const std::string& type_str,
                                                         int compression_level = 6);
    
    static CompressionType parse_type(const std::string& type_str);
};

} // namespace compression
} // namespace logpipeline
