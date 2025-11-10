#pragma once

#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <asio.hpp>

namespace logpipeline {
namespace protocol {

struct Message;

} // namespace protocol

class AgentTCPClient {
public:
    explicit AgentTCPClient(const std::vector<std::string>& forwarders);
    ~AgentTCPClient();
    
    bool start();
    void stop();
    
    bool send_data(uint64_t task_id, uint64_t offset, const std::string& data);
    bool send_metrics(const std::string& metrics_json);
    
    bool is_connected() const;
    
private:
    struct PendingData {
        uint64_t task_id;
        uint64_t offset;
        std::string data;
        std::promise<bool> promise;
    };
    
    std::vector<std::string> forwarders_;
    std::atomic<bool> running_;
    
    // Asio components
    std::unique_ptr<asio::io_context> io_context_;
    std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>> work_guard_;
    std::thread io_thread_;
    
    // Connection management
    std::unique_ptr<asio::ip::tcp::socket> socket_;
    std::string current_host_;
    int current_port_;
    std::atomic<bool> connected_;
    
    // Pending data queue
    mutable std::mutex pending_mutex_;
    std::queue<PendingData> pending_data_;
    std::condition_variable pending_cv_;
    std::thread sender_thread_;
    
    // Heartbeat
    std::unique_ptr<asio::steady_timer> heartbeat_timer_;
    std::atomic<uint64_t> last_heartbeat_sent_{0};
    
    bool connect();
    void disconnect();
    bool try_connect(const std::string& host, int port);
    
    void start_io_thread();
    void start_sender_thread();
    void start_heartbeat();
    
    bool send_message(const protocol::Message& msg);
    void handle_send_data();
    void handle_response(const protocol::Message& response);
    
    std::pair<std::string, int> parse_address(const std::string& address);
};

} // namespace logpipeline
