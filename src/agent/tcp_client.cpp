#include "agent/tcp_client.h"
#include "common/protocol.h"
#include <spdlog/spdlog.h>
#include <chrono>
#include <random>

namespace logpipeline {

AgentTCPClient::AgentTCPClient(const std::vector<std::string>& forwarders)
    : forwarders_(forwarders), running_(false), connected_(false) {
}

AgentTCPClient::~AgentTCPClient() {
    stop();
}

bool AgentTCPClient::start() {
    if (running_) {
        return true;
    }
    
    running_ = true;
    
    io_context_ = std::make_unique<asio::io_context>();
    work_guard_ = std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(
        io_context_->get_executor());
    
    start_io_thread();
    start_sender_thread();
    
    // Try to connect
    if (connect()) {
        start_heartbeat();
    }
    
    spdlog::info("AgentTCPClient started");
    return true;
}

void AgentTCPClient::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    disconnect();
    
    if (io_context_) {
        io_context_->stop();
    }
    
    if (io_thread_.joinable()) {
        io_thread_.join();
    }
    
    if (sender_thread_.joinable()) {
        sender_thread_.join();
    }
    
    spdlog::info("AgentTCPClient stopped");
}

bool AgentTCPClient::send_data(uint64_t task_id, uint64_t offset, const std::string& data) {
    if (!connected_) {
        return false;
    }
    
    auto msg = protocol::MessageBuilder::create_data(task_id, offset, data);
    return send_message(msg);
}

bool AgentTCPClient::send_metrics(const std::string& metrics_json) {
    if (!connected_) {
        return false;
    }
    
    auto msg = protocol::MessageBuilder::create_metrics(metrics_json);
    return send_message(msg);
}

bool AgentTCPClient::is_connected() const {
    return connected_;
}

bool AgentTCPClient::connect() {
    if (forwarders_.empty()) {
        spdlog::error("No forwarders configured");
        return false;
    }
    
    // Try each forwarder in order
    for (const auto& forwarder : forwarders_) {
        auto [host, port] = parse_address(forwarder);
        if (try_connect(host, port)) {
            current_host_ = host;
            current_port_ = port;
            connected_ = true;
            spdlog::info("Connected to forwarder: {}:{}", host, port);
            return true;
        }
    }
    
    spdlog::error("Failed to connect to any forwarder");
    return false;
}

void AgentTCPClient::disconnect() {
    if (socket_ && socket_->is_open()) {
        asio::error_code ec;
        socket_->close(ec);
        if (ec) {
            spdlog::error("Error closing socket: {}", ec.message());
        }
    }
    
    connected_ = false;
}

bool AgentTCPClient::try_connect(const std::string& host, int port) {
    try {
        if (!socket_) {
            socket_ = std::make_unique<asio::ip::tcp::socket>(*io_context_);
        }
        
        asio::ip::tcp::resolver resolver(*io_context_);
        auto endpoints = resolver.resolve(host, std::to_string(port));
        
        asio::error_code ec;
        asio::connect(*socket_, endpoints, ec);
        
        if (ec) {
            spdlog::warn("Failed to connect to {}:{}: {}", host, port, ec.message());
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        spdlog::error("Exception during connect to {}:{}: {}", host, port, e.what());
        return false;
    }
}

void AgentTCPClient::start_io_thread() {
    io_thread_ = std::thread([this]() {
        try {
            io_context_->run();
        } catch (const std::exception& e) {
            spdlog::error("IO thread error: {}", e.what());
        }
    });
}

void AgentTCPClient::start_sender_thread() {
    sender_thread_ = std::thread([this]() {
        while (running_) {
            handle_send_data();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
}

void AgentTCPClient::start_heartbeat() {
    heartbeat_timer_ = std::make_unique<asio::steady_timer>(*io_context_);
    
    std::weak_ptr<AgentTCPClient> weak_self; // Simplified for now
    
    auto heartbeat_callback = [this](const asio::error_code& ec) {
        if (!ec && running_ && connected_) {
            auto msg = protocol::MessageBuilder::create_heartbeat();
            send_message(msg);
            
            // Schedule next heartbeat
            heartbeat_timer_->expires_after(std::chrono::seconds(30));
            heartbeat_timer_->async_wait([this](const asio::error_code& ec) {
                if (!ec) start_heartbeat();
            });
        }
    };
    
    heartbeat_timer_->expires_after(std::chrono::seconds(30));
    heartbeat_timer_->async_wait(heartbeat_callback);
}

bool AgentTCPClient::send_message(const protocol::Message& msg) {
    try {
        auto serialized = protocol::MessageBuilder::serialize(msg);
        
        asio::error_code ec;
        asio::write(*socket_, asio::buffer(serialized), ec);
        
        if (ec) {
            spdlog::error("Failed to send message: {}", ec.message());
            connected_ = false;
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        spdlog::error("Exception during send_message: {}", e.what());
        connected_ = false;
        return false;
    }
}

void AgentTCPClient::handle_send_data() {
    std::unique_lock<std::mutex> lock(pending_mutex_);
    
    if (pending_data_.empty()) {
        pending_cv_.wait_for(lock, std::chrono::milliseconds(100));
        return;
    }
    
    auto pending = std::move(pending_data_.front());
    pending_data_.pop();
    lock.unlock();
    
    if (send_data(pending.task_id, pending.offset, pending.data)) {
        pending.promise.set_value(true);
    } else {
        pending.promise.set_value(false);
    }
}

std::pair<std::string, int> AgentTCPClient::parse_address(const std::string& address) {
    size_t colon_pos = address.find(':');
    if (colon_pos == std::string::npos) {
        return {address, 9090}; // Default port
    }
    
    std::string host = address.substr(0, colon_pos);
    int port = std::stoi(address.substr(colon_pos + 1));
    
    return {host, port};
}

} // namespace logpipeline
