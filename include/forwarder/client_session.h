#pragma once

#include <string>
#include <memory>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <asio.hpp>

namespace logpipeline {
namespace protocol {

struct Message;

} // namespace protocol

class KafkaProducer;
class MetricsAggregator;

class ClientSession : public std::enable_shared_from_this<ClientSession> {
public:
    ClientSession(asio::ip::tcp::socket socket,
                  KafkaProducer* kafka_producer,
                  MetricsAggregator* metrics_aggregator);
    ~ClientSession();
    
    void start();
    void stop();
    
    std::string get_remote_address() const;
    asio::ip::tcp::socket& get_socket() { return socket_; }
    
private:
    static constexpr size_t BUFFER_SIZE = 64 * 1024; // 64KB
    static constexpr size_t HIGH_WATERMARK_BYTES = 64 * 1024 * 1024; // 64MB
    static constexpr size_t LOW_WATERMARK_BYTES = 32 * 1024 * 1024;  // 32MB
    
    asio::ip::tcp::socket socket_;
    KafkaProducer* kafka_producer_;
    MetricsAggregator* metrics_aggregator_;
    
    std::atomic<bool> running_{false};
    std::string agent_id_;
    
    // Buffer management
    std::array<uint8_t, BUFFER_SIZE> read_buffer_;
    std::queue<std::vector<uint8_t>> write_queue_;
    mutable std::mutex write_mutex_;
    std::atomic<bool> writing_{false};
    std::atomic<size_t> buffer_bytes_{0};
    std::atomic<uint64_t> buffer_full_since_{0};
    
    void start_read();
    void handle_read(const asio::error_code& error, size_t bytes_transferred);
    
    void process_message(const protocol::Message& msg);
    void process_data_message(const protocol::Message& msg);
    void process_metrics_message(const protocol::Message& msg);
    void process_heartbeat_message(const protocol::Message& msg);
    
    void send_ack(uint64_t task_id, uint64_t offset);
    void send_message(const protocol::Message& msg);
    void handle_write(const asio::error_code& error, size_t bytes_transferred);
    
    bool should_apply_backpressure() const;
    void check_backpressure_timeout();
};

} // namespace logpipeline
