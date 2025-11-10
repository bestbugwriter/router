#include "common/protocol.h"
#include <cstring>
#include <sstream>

namespace logpipeline {
namespace protocol {

Message MessageBuilder::create_data(uint64_t task_id, uint64_t offset, const std::string& data) {
    Message msg;
    msg.header.magic = MAGIC_NUMBER;
    msg.header.version = VERSION;
    msg.header.type = static_cast<uint16_t>(MessageType::DATA);
    msg.header.length = static_cast<uint32_t>(data.size());
    msg.header.task_id = task_id;
    msg.header.offset = offset;
    msg.payload.assign(data.begin(), data.end());
    return msg;
}

Message MessageBuilder::create_ack(uint64_t task_id, uint64_t offset) {
    Message msg;
    msg.header.magic = MAGIC_NUMBER;
    msg.header.version = VERSION;
    msg.header.type = static_cast<uint16_t>(MessageType::ACK);
    msg.header.length = 0;
    msg.header.task_id = task_id;
    msg.header.offset = offset;
    return msg;
}

Message MessageBuilder::create_heartbeat() {
    Message msg;
    msg.header.magic = MAGIC_NUMBER;
    msg.header.version = VERSION;
    msg.header.type = static_cast<uint16_t>(MessageType::HEARTBEAT);
    msg.header.length = 0;
    msg.header.task_id = 0;
    msg.header.offset = 0;
    return msg;
}

Message MessageBuilder::create_metrics(const std::string& metrics_json) {
    Message msg;
    msg.header.magic = MAGIC_NUMBER;
    msg.header.version = VERSION;
    msg.header.type = static_cast<uint16_t>(MessageType::METRICS);
    msg.header.length = static_cast<uint32_t>(metrics_json.size());
    msg.header.task_id = 0;
    msg.header.offset = 0;
    msg.payload.assign(metrics_json.begin(), metrics_json.end());
    return msg;
}

std::vector<uint8_t> MessageBuilder::serialize(const Message& msg) {
    std::vector<uint8_t> data;
    data.resize(sizeof(Header) + msg.payload.size());
    
    std::memcpy(data.data(), &msg.header, sizeof(Header));
    if (!msg.payload.empty()) {
        std::memcpy(data.data() + sizeof(Header), msg.payload.data(), msg.payload.size());
    }
    
    return data;
}

Message MessageBuilder::deserialize(const std::vector<uint8_t>& data) {
    Message msg;
    
    if (data.size() < sizeof(Header)) {
        throw std::runtime_error("Invalid message: data too small");
    }
    
    std::memcpy(&msg.header, data.data(), sizeof(Header));
    
    if (msg.header.magic != MAGIC_NUMBER) {
        throw std::runtime_error("Invalid message: wrong magic number");
    }
    
    if (msg.header.version != VERSION) {
        throw std::runtime_error("Invalid message: unsupported version");
    }
    
    if (data.size() < sizeof(Header) + msg.header.length) {
        throw std::runtime_error("Invalid message: incomplete payload");
    }
    
    msg.payload.resize(msg.header.length);
    if (msg.header.length > 0) {
        std::memcpy(msg.payload.data(), data.data() + sizeof(Header), msg.header.length);
    }
    
    return msg;
}

} // namespace protocol
} // namespace logpipeline
