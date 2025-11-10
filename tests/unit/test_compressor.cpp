#include <cassert>
#include <iostream>
#include "common/compressor.h"

using namespace logpipeline::compression;

// 简单的单元测试（不使用外部测试框架）

void test_no_compressor() {
    NoCompressor compressor;
    std::string input = "Hello, World!";
    std::vector<uint8_t> input_data(input.begin(), input.end());
    std::vector<uint8_t> compressed, decompressed;
    
    // 测试压缩
    assert(compressor.compress(input_data, compressed));
    assert(compressed == input_data);  // 无压缩应该返回相同的数据
    
    // 测试解压
    assert(compressor.decompress(compressed, decompressed));
    assert(decompressed == input_data);
    
    std::cout << "✓ NoCompressor test passed" << std::endl;
}

void test_zlib_compressor() {
#ifdef HAVE_ZLIB
    ZlibCompressor compressor(6);
    
    // 创建测试数据（重复内容便于压缩）
    std::string input;
    for (int i = 0; i < 100; ++i) {
        input += "This is test data for compression. ";
    }
    
    std::vector<uint8_t> input_data(input.begin(), input.end());
    std::vector<uint8_t> compressed, decompressed;
    
    // 测试压缩
    assert(compressor.compress(input_data, compressed));
    assert(compressed.size() < input_data.size());  // 应该被压缩
    
    // 测试解压
    assert(compressor.decompress(compressed, decompressed));
    assert(decompressed == input_data);
    
    // 计算压缩率
    double ratio = 100.0 * compressed.size() / input_data.size();
    std::cout << "✓ ZlibCompressor test passed (compression ratio: " << ratio << "%)" << std::endl;
#else
    std::cout << "⊘ ZlibCompressor test skipped (zlib not available)" << std::endl;
#endif
}

void test_snappy_compressor() {
#ifdef HAVE_SNAPPY
    SnappyCompressor compressor;
    
    // 创建测试数据
    std::string input;
    for (int i = 0; i < 100; ++i) {
        input += "This is test data for snappy compression. ";
    }
    
    std::vector<uint8_t> input_data(input.begin(), input.end());
    std::vector<uint8_t> compressed, decompressed;
    
    // 测试压缩
    assert(compressor.compress(input_data, compressed));
    assert(compressed.size() < input_data.size());  // 应该被压缩
    
    // 测试解压
    assert(compressor.decompress(compressed, decompressed));
    assert(decompressed == input_data);
    
    // 计算压缩率
    double ratio = 100.0 * compressed.size() / input_data.size();
    std::cout << "✓ SnappyCompressor test passed (compression ratio: " << ratio << "%)" << std::endl;
#else
    std::cout << "⊘ SnappyCompressor test skipped (snappy not available)" << std::endl;
#endif
}

void test_compressor_factory() {
    // 测试工厂类
    auto no_comp = CompressorFactory::create(CompressionType::NONE);
    assert(no_comp != nullptr);
    assert(no_comp->get_type() == CompressionType::NONE);
    
    auto from_string = CompressorFactory::create_from_string("zlib");
    assert(from_string != nullptr);
    
    std::cout << "✓ CompressorFactory test passed" << std::endl;
}

int main() {
    std::cout << "Running compressor unit tests..." << std::endl;
    
    try {
        test_no_compressor();
        test_zlib_compressor();
        test_snappy_compressor();
        test_compressor_factory();
        
        std::cout << "\n✓ All compressor tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "✗ Test failed: " << e.what() << std::endl;
        return 1;
    }
}
