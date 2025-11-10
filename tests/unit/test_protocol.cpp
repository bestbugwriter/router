#include <cassert>
#include <iostream>
#include "common/protocol.h"

using namespace logpipeline::protocol;

void test_message_builder() {
    // 测试创建数据消息
    auto msg = MessageBuilder::create_data(12345, 100, "Test payload");
    
    assert(msg.header.magic == MAGIC_NUMBER);
    assert(msg.header.version == VERSION);
    assert(msg.header.type == static_cast<uint16_t>(MessageType::DATA));
    assert(msg.header.task_id == 12345);
    assert(msg.header.offset == 100);
    assert(msg.header.length == 12);  // "Test payload" 的长度
    
    std::string payload(msg.payload.begin(), msg.payload.end());
    assert(payload == "Test payload");
    
    std::cout << "✓ MessageBuilder::create_data test passed" << std::endl;
}

void test_message_serialization() {
    // 创建消息
    std::string data = "Hello, World!";
    auto msg = MessageBuilder::create_data(999, 50, data);
    
    // 序列化
    auto serialized = MessageBuilder::serialize(msg);
    
    // 验证序列化大小
    assert(serialized.size() == sizeof(Header) + data.size());
    
    // 验证 magic 和 version
    uint32_t magic;
    std::memcpy(&magic, serialized.data(), sizeof(uint32_t));
    assert(magic == MAGIC_NUMBER);
    
    std::cout << "✓ Message serialization test passed" << std::endl;
}

void test_message_deserialization() {
    // 创建原始消息
    std::string data = "Test message data";
    auto original = MessageBuilder::create_data(1234, 567, data);
    
    // 序列化
    auto serialized = MessageBuilder::serialize(original);
    
    // 反序列化
    auto deserialized = MessageBuilder::deserialize(serialized);
    
    // 验证
    assert(deserialized.header.task_id == original.header.task_id);
    assert(deserialized.header.offset == original.header.offset);
    assert(deserialized.header.length == original.header.length);
    
    std::string payload(deserialized.payload.begin(), deserialized.payload.end());
    assert(payload == data);
    
    std::cout << "✓ Message deserialization test passed" << std::endl;
}

void test_ack_message() {
    auto ack = MessageBuilder::create_ack(999, 50);
    
    assert(ack.header.type == static_cast<uint16_t>(MessageType::ACK));
    assert(ack.header.task_id == 999);
    assert(ack.header.offset == 50);
    assert(ack.header.length == 0);
    assert(ack.payload.empty());
    
    std::cout << "✓ ACK message test passed" << std::endl;
}

void test_heartbeat_message() {
    auto hb = MessageBuilder::create_heartbeat();
    
    assert(hb.header.type == static_cast<uint16_t>(MessageType::HEARTBEAT));
    assert(hb.header.length == 0);
    assert(hb.payload.empty());
    
    std::cout << "✓ Heartbeat message test passed" << std::endl;
}

void test_metrics_message() {
    std::string metrics_json = R"({"lines_collected": 1000, "bytes_collected": 50000})";
    auto metrics = MessageBuilder::create_metrics(metrics_json);
    
    assert(metrics.header.type == static_cast<uint16_t>(MessageType::METRICS));
    assert(metrics.header.length == metrics_json.size());
    
    std::string payload(metrics.payload.begin(), metrics.payload.end());
    assert(payload == metrics_json);
    
    std::cout << "✓ Metrics message test passed" << std::endl;
}

int main() {
    std::cout << "Running protocol unit tests..." << std::endl;
    
    try {
        test_message_builder();
        test_message_serialization();
        test_message_deserialization();
        test_ack_message();
        test_heartbeat_message();
        test_metrics_message();
        
        std::cout << "\n✓ All protocol tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "✗ Test failed: " << e.what() << std::endl;
        return 1;
    }
}
