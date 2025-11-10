#include "router/client_session.h"
#include "router/metrics_aggregator.h"
#include "common/message_queue.h"
#include "common/protocol.h"
#include "router/metrics_aggregator.h"
#include <spdlog/spdlog.h>
#include <chrono>

namespace logpipeline {

RouterClientSession::RouterClientSession(asio::ip::tcp::socket socket,
                                        RouterMetricsAggregator* metrics_aggregator,
                                        const TaskConfigMap& task_config)
    : socket_(std::move(socket)),
      metrics_aggregator_(metrics_aggregator),
      task_config_map_(task_config),
      running_(false) {
}

RouterClientSession::~RouterClientSession() {
    stop();
}

void RouterClientSession::start() {
    if (running_) {
        return;
    }
    
    running_ = true;
    spdlog::debug("Starting Router client session from {}", get_remote_address());
    start_read();
}

void RouterClientSession::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    asio::error_code ec;
    socket_.close(ec);
    
    spdlog::debug("Stopped Router client session");
}

std::string RouterClientSession::get_remote_address() const {
    try {
        return socket_.remote_endpoint().address().to_string() + ":" +
               std::to_string(socket_.remote_endpoint().port());
    } catch (const std::exception&) {
        return "unknown";
    }
}

void RouterClientSession::start_read() {
    if (!running_) {
        return;
    }
    
    auto self = shared_from_this();
    socket_.async_read_some(
        asio::buffer(read_buffer_),
        [this, self](const asio::error_code& error, size_t bytes_transferred) {
            handle_read(error, bytes_transferred);
        });
}

void RouterClientSession::handle_read(const asio::error_code& error, 
                                     size_t bytes_transferred) {
    if (!running_) {
        return;
    }
    
    if (error) {
        spdlog::debug("Read error from {}: {}", get_remote_address(),
                     error.message());
        stop();
        return;
    }
    
    if (bytes_transferred == 0) {
        stop();
        return;
    }
    
    // 处理接收到的数据
    try {
        std::vector<uint8_t> data(read_buffer_.begin(),
                                  read_buffer_.begin() + bytes_transferred);
        
        // 解析协议消息
        auto message = protocol::MessageBuilder::deserialize(data);
        process_message(message);
    } catch (const std::exception& e) {
        spdlog::error("Error processing message from {}: {}",
                     get_remote_address(), e.what());
    }
    
    start_read();
}

void RouterClientSession::process_message(const protocol::Message& msg) {
    using MessageType = protocol::MessageType;
    
    switch (static_cast<MessageType>(msg.header.type)) {
        case MessageType::DATA:
            process_data_message(msg);
            break;
        case MessageType::HEARTBEAT:
            process_heartbeat_message(msg);
            break;
        case MessageType::METRICS:
            process_metrics_message(msg);
            break;
        case MessageType::ACK:
            // Router 不需要处理 ACK
            spdlog::debug("Received ACK from Agent");
            break;
        default:
            spdlog::warn("Unknown message type: {}", msg.header.type);
    }
}

void RouterClientSession::process_data_message(const protocol::Message& msg) {
    std::string task_id_str = std::to_string(msg.header.task_id);
    spdlog::debug("Processing data message: task_id={}, offset={}, size={}",
                 task_id_str, msg.header.offset, msg.payload.size());

    auto task_it = task_config_map_.find(task_id_str);
    if (task_it == task_config_map_.end()) {
        spdlog::warn("No task configuration found for task_id: {}", task_id_str);
        return;
    }

    const auto& task_info = task_it->second;

    if (producer_getter_) {
        auto producer = producer_getter_(task_info.cluster_id);
        if (producer) {
            logpipeline::MQMessage mq_msg;
            mq_msg.topic = task_info.topic;
            mq_msg.payload = msg.payload;
            mq_msg.task_id = msg.header.task_id;
            mq_msg.offset = msg.header.offset;
            
            if (!producer->send(mq_msg)) {
                spdlog::error("Failed to send message to Kafka for task {}", task_id_str);
            } else {
                spdlog::debug("Successfully sent message for task {} to topic {}", task_id_str, task_info.topic);
            }
        } else {
            spdlog::warn("No producer found for cluster '{}'", task_info.cluster_id);
        }
    }
    
    // Send ACK back to agent
    send_ack(msg.header.task_id, msg.header.offset);
}

void RouterClientSession::process_metrics_message(const protocol::Message& msg) {
    // Agent 上报的监控指标
    std::string metrics_json(msg.payload.begin(), msg.payload.end());
    
    spdlog::debug("Processing metrics message from Agent {}, size={}",
                 agent_id_, metrics_json.size());
    
    if (metrics_aggregator_) {
        metrics_aggregator_->update_agent_metrics(agent_id_, metrics_json);
    }
}

void RouterClientSession::process_heartbeat_message(const protocol::Message& msg) {
    spdlog::debug("Received heartbeat from Agent {}", agent_id_);
    // 心跳消息，更新连接状态
}

void RouterClientSession::send_ack(uint64_t task_id, uint64_t offset) {
    auto ack_msg = protocol::MessageBuilder::create_ack(task_id, offset);
    send_message(ack_msg);
}

void RouterClientSession::send_message(const protocol::Message& msg) {
    auto serialized = protocol::MessageBuilder::serialize(msg);
    
    {
        std::lock_guard<std::mutex> lock(write_mutex_);
        write_queue_.push(serialized);
    }
    
    if (!writing_.exchange(true)) {
        handle_write(asio::error_code(), 0);
    }
}

void RouterClientSession::handle_write(const asio::error_code& error,
                                      size_t bytes_transferred) {
    if (!running_) {
        return;
    }
    
    if (error) {
        spdlog::error("Write error: {}", error.message());
        stop();
        return;
    }
    
    std::lock_guard<std::mutex> lock(write_mutex_);
    
    if (write_queue_.empty()) {
        writing_ = false;
        return;
    }
    
    auto& msg = write_queue_.front();
    auto self = shared_from_this();
    
    asio::async_write(
        socket_,
        asio::buffer(msg),
        [this, self](const asio::error_code& error, size_t bytes_transferred) {
            {
                std::lock_guard<std::mutex> lock(write_mutex_);
                if (!write_queue_.empty()) {
                    write_queue_.pop();
                }
            }
            handle_write(error, bytes_transferred);
        });
}

bool RouterClientSession::should_apply_backpressure() const {
    return buffer_bytes_.load() > HIGH_WATERMARK_BYTES;
}

void RouterClientSession::check_backpressure_timeout() {
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    auto since = buffer_full_since_.load();
    
    if (since > 0) {
        auto duration = (now - since) / 1e9;  // 转换为秒
        if (duration > 60) {  // 如果缓冲区满超过 60 秒
            spdlog::warn("Backpressure timeout, closing connection");
            stop();
        }
    }
}

} // namespace logpipeline
