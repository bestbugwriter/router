#pragma once

#include <cstdint>
#include <vector>
#include <string>

namespace logpipeline {
namespace protocol {

constexpr uint32_t MAGIC_NUMBER = 0xCAFEBABE;
constexpr uint16_t VERSION = 0x0100;

enum class MessageType : uint16_t {
    DATA = 0x01,
    ACK = 0x02,
    HEARTBEAT = 0x03,
    METRICS = 0x04
};

#pragma pack(push, 1)
struct Header {
    uint32_t magic;
    uint16_t version;
    uint16_t type;
    uint32_t length;
    uint64_t task_id;
    uint64_t offset;
};
#pragma pack(pop)

struct Message {
    Header header;
    std::vector<uint8_t> payload;
};

class MessageBuilder {
public:
    static Message create_data(uint64_t task_id, uint64_t offset, const std::string& data);
    static Message create_ack(uint64_t task_id, uint64_t offset);
    static Message create_heartbeat();
    static Message create_metrics(const std::string& metrics_json);
    
    static std::vector<uint8_t> serialize(const Message& msg);
    static Message deserialize(const std::vector<uint8_t>& data);
};

} // namespace protocol
} // namespace logpipeline
