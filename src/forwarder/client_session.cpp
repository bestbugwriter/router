#include "forwarder/client_session.h"
#include "forwarder/kafka_producer.h"
#include "forwarder/metrics_aggregator.h"
#include "common/protocol.h"
#include <spdlog/spdlog.h>
#include <chrono>

namespace logpipeline {

ClientSession::ClientSession(asio::ip::tcp::socket socket,
                             KafkaProducer* kafka_producer,
                             MetricsAggregator* metrics_aggregator)
    : socket_(std::move(socket)),
      kafka_producer_(kafka_producer),
      metrics_aggregator_(metrics_aggregator) {
}

ClientSession::~ClientSession() {
    stop();
}

void ClientSession::start() {
    running_ = true;
    start_read();
}

void ClientSession::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    asio::error_code ec;
    socket_.close(ec);
    
    // Update metrics
    if (metrics_aggregator_) {
        metrics_aggregator_->set_active_connections(-1); // Will be handled by TCPServer
    }
    
    spdlog::debug("Client session stopped: {}", get_remote_address());
}

std::string ClientSession::get_remote_address() const {
    try {
        return socket_.remote_endpoint().address().to_string() + ":" + 
               std::to_string(socket_.remote_endpoint().port());
    } catch (...) {
        return "unknown";
    }
}

void ClientSession::start_read() {
    if (!running_) {
        return;
    }
    
    socket_.async_read_some(asio::buffer(read_buffer_),
        [this](const asio::error_code& error, size_t bytes_transferred) {
            handle_read(error, bytes_transferred);
        });
}

void ClientSession::handle_read(const asio::error_code& error, size_t bytes_transferred) {
    if (!running_) {
        return;
    }
    
    if (error) {
        if (error != asio::error::eof) {
            spdlog::error("Read error from {}: {}", get_remote_address(), error.message());
        }
        stop();
        return;
    }
    
    try {
        // Process received data
        static thread_local std::vector<uint8_t> receive_buffer;
        receive_buffer.insert(receive_buffer.end(), read_buffer_.begin(), 
                             read_buffer_.begin() + bytes_transferred);
        
        // Try to extract complete messages
        while (receive_buffer.size() >= sizeof(protocol::Header)) {
            // Parse header
            protocol::Header header;
            std::memcpy(&header, receive_buffer.data(), sizeof(header));
            
            // Validate magic and version
            if (header.magic != protocol::MAGIC_NUMBER || 
                header.version != protocol::VERSION) {
                spdlog::error("Invalid protocol header from {}", get_remote_address());
                stop();
                return;
            }
            
            size_t total_message_size = sizeof(header) + header.length;
            
            if (receive_buffer.size() < total_message_size) {
                // Not enough data for complete message
                break;
            }
            
            // Extract message
            std::vector<uint8_t> message_data(receive_buffer.begin(), 
                                             receive_buffer.begin() + total_message_size);
            receive_buffer.erase(receive_buffer.begin(), 
                                receive_buffer.begin() + total_message_size);
            
            // Parse and process message
            auto msg = protocol::MessageBuilder::deserialize(message_data);
            process_message(msg);
        }
        
        // Continue reading
        start_read();
        
    } catch (const std::exception& e) {
        spdlog::error("Error processing data from {}: {}", get_remote_address(), e.what());
        stop();
        return;
    }
}

void ClientSession::process_message(const protocol::Message& msg) {
    switch (static_cast<protocol::MessageType>(msg.header.type)) {
        case protocol::MessageType::DATA:
            process_data_message(msg);
            break;
            
        case protocol::MessageType::METRICS:
            process_metrics_message(msg);
            break;
            
        case protocol::MessageType::HEARTBEAT:
            process_heartbeat_message(msg);
            break;
            
        case protocol::MessageType::ACK:
            // Should not receive ACK from agent
            spdlog::warn("Unexpected ACK message from {}", get_remote_address());
            break;
            
        default:
            spdlog::warn("Unknown message type {} from {}", 
                        static_cast<int>(msg.header.type), get_remote_address());
            break;
    }
}

void ClientSession::process_data_message(const protocol::Message& msg) {
    // Check backpressure
    if (should_apply_backpressure()) {
        check_backpressure_timeout();
        return; // Drop the message
    }
    
    if (!kafka_producer_) {
        spdlog::error("Kafka producer not available");
        return;
    }
    
    // Create Kafka message
    KafkaMessage kafka_msg;
    kafka_msg.topic = "logs"; // Should be determined by task configuration
    kafka_msg.key = std::to_string(msg.header.task_id);
    kafka_msg.payload.assign(msg.payload.begin(), msg.payload.end());
    kafka_msg.task_id = msg.header.task_id;
    kafka_msg.offset = msg.header.offset;
    kafka_msg.callback = [this, msg](bool success) {
        if (success) {
            send_ack(msg.header.task_id, msg.header.offset);
        } else {
            spdlog::error("Failed to send message to Kafka for task {}", msg.header.task_id);
            // Could implement retry logic here
        }
    };
    
    if (kafka_producer_->send_message(kafka_msg)) {
        buffer_bytes_ += msg.payload.size();
    } else {
        spdlog::error("Failed to queue message for Kafka");
    }
}

void ClientSession::process_metrics_message(const protocol::Message& msg) {
    if (!metrics_aggregator_) {
        return;
    }
    
    std::string metrics_json(msg.payload.begin(), msg.payload.end());
    
    try {
        // Extract agent_id from session or metrics data
        std::string agent_id = get_remote_address(); // Simplified
        metrics_aggregator_->update_agent_metrics(agent_id, metrics_json);
        
        spdlog::debug("Received metrics from {}: {}", agent_id, metrics_json);
        
    } catch (const std::exception& e) {
        spdlog::error("Error processing metrics from {}: {}", get_remote_address(), e.what());
    }
}

void ClientSession::process_heartbeat_message(const protocol::Message& msg) {
    spdlog::debug("Received heartbeat from {}", get_remote_address());
    // No response needed for heartbeat
}

void ClientSession::send_ack(uint64_t task_id, uint64_t offset) {
    auto ack_msg = protocol::MessageBuilder::create_ack(task_id, offset);
    send_message(ack_msg);
}

void ClientSession::send_message(const protocol::Message& msg) {
    auto serialized = protocol::MessageBuilder::serialize(msg);
    
    std::lock_guard<std::mutex> lock(write_mutex_);
    write_queue_.push(serialized);
    
    if (!writing_) {
        writing_ = true;
        
        auto self = shared_from_this();
        asio::async_write(socket_, asio::buffer(write_queue_.front()),
            [this, self](const asio::error_code& error, size_t bytes_transferred) {
                std::lock_guard<std::mutex> lock(write_mutex_);
                write_queue_.pop();
                
                if (error) {
                    spdlog::error("Write error to {}: {}", get_remote_address(), error.message());
                    stop();
                    return;
                }
                
                // Continue writing if there are more messages
                if (!write_queue_.empty()) {
                    auto next_msg = write_queue_.front();
                    asio::async_write(socket_, asio::buffer(next_msg),
                        [this, self](const asio::error_code& error, size_t bytes_transferred) {
                            handle_write(error, bytes_transferred);
                        });
                } else {
                    writing_ = false;
                }
            });
    }
}

void ClientSession::handle_write(const asio::error_code& error, size_t bytes_transferred) {
    if (error) {
        spdlog::error("Write error to {}: {}", get_remote_address(), error.message());
        stop();
        return;
    }
    
    std::lock_guard<std::mutex> lock(write_mutex_);
    
    if (!write_queue_.empty()) {
        write_queue_.pop();
        
        if (!write_queue_.empty()) {
            auto next_msg = write_queue_.front();
            auto self = shared_from_this();
            asio::async_write(socket_, asio::buffer(next_msg),
                [this, self](const asio::error_code& error, size_t bytes_transferred) {
                    handle_write(error, bytes_transferred);
                });
        } else {
            writing_ = false;
        }
    } else {
        writing_ = false;
    }
}

bool ClientSession::should_apply_backpressure() const {
    return buffer_bytes_ > HIGH_WATERMARK_BYTES;
}

void ClientSession::check_backpressure_timeout() {
    auto now = std::chrono::steady_clock::now();
    
    if (buffer_full_since_ == 0) {
        buffer_full_since_ = now.time_since_epoch().count();
    } else {
        auto duration = now.time_since_epoch().count() - buffer_full_since_;
        if (duration > 30 * 1000000000) { // 30 seconds in nanoseconds
            spdlog::warn("Buffer full for too long, dropping connection: {}", get_remote_address());
            stop();
        }
    }
}

} // namespace logpipeline
