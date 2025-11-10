#include "router/metrics_endpoint.h"
#include "router/metrics_aggregator.h"
#include <spdlog/spdlog.h>
#include <sstream>
#include <regex>

namespace logpipeline {

class RouterMetricsEndpoint::HttpRequestHandler {
public:
    static void handle_request(asio::ip::tcp::socket& socket,
                              RouterMetricsAggregator* aggregator) {
        std::array<uint8_t, 1024> buffer;
        asio::error_code error;
        
        size_t bytes = socket.read_some(asio::buffer(buffer), error);
        if (error) {
            return;
        }
        
        std::string request(buffer.begin(), buffer.begin() + bytes);
        
        // 简单的 HTTP 请求解析
        std::istringstream iss(request);
        std::string method, path, version;
        iss >> method >> path >> version;
        
        std::string response;
        std::string content;
        
        if (path == "/metrics") {
            content = aggregator->get_metrics_text();
            response = "HTTP/1.1 200 OK\r\n"
                      "Content-Type: text/plain; version=0.0.4\r\n"
                      "Content-Length: " + std::to_string(content.length()) + "\r\n"
                      "\r\n" + content;
        } else if (path == "/metrics/json") {
            content = aggregator->get_metrics_json();
            response = "HTTP/1.1 200 OK\r\n"
                      "Content-Type: application/json\r\n"
                      "Content-Length: " + std::to_string(content.length()) + "\r\n"
                      "\r\n" + content;
        } else if (path == "/health") {
            content = "{\"status\": \"up\"}";
            response = "HTTP/1.1 200 OK\r\n"
                      "Content-Type: application/json\r\n"
                      "Content-Length: " + std::to_string(content.length()) + "\r\n"
                      "\r\n" + content;
        } else {
            response = "HTTP/1.1 404 Not Found\r\n"
                      "Content-Length: 0\r\n"
                      "\r\n";
        }
        
        asio::write(socket, asio::buffer(response), error);
    }
};

RouterMetricsEndpoint::RouterMetricsEndpoint(const std::string& listen_address,
                                           RouterMetricsAggregator* aggregator)
    : listen_address_(listen_address),
      aggregator_(aggregator),
      running_(false) {
    
    // 解析 listen_address
    size_t colon_pos = listen_address.find(':');
    if (colon_pos != std::string::npos) {
        host_ = listen_address.substr(0, colon_pos);
        port_ = std::stoi(listen_address.substr(colon_pos + 1));
    } else {
        host_ = listen_address;
        port_ = 9101;  // 默认 Prometheus 端口
    }
}

RouterMetricsEndpoint::~RouterMetricsEndpoint() {
    stop();
}

bool RouterMetricsEndpoint::start() {
    spdlog::info("Starting Router metrics endpoint on {}:{}", host_, port_);
    
    try {
        io_context_ = std::make_unique<asio::io_context>(1);
        
        asio::ip::tcp::endpoint endpoint(
            asio::ip::make_address(host_), port_);
        
        acceptor_ = std::make_unique<asio::ip::tcp::acceptor>(
            *io_context_, endpoint);
        
        running_ = true;
        
        io_thread_ = std::thread([this]() {
            io_context_->run();
        });
        
        start_accept();
        
        spdlog::info("Router metrics endpoint started successfully");
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to start metrics endpoint: {}", e.what());
        return false;
    }
}

void RouterMetricsEndpoint::stop() {
    if (!running_) {
        return;
    }
    
    spdlog::info("Stopping Router metrics endpoint...");
    running_ = false;
    
    if (acceptor_) {
        acceptor_->close();
    }
    
    if (io_context_) {
        io_context_->stop();
    }
    
    if (io_thread_.joinable()) {
        io_thread_.join();
    }
    
    spdlog::info("Router metrics endpoint stopped");
}

void RouterMetricsEndpoint::start_accept() {
    if (!running_) {
        return;
    }
    
    auto socket = std::make_shared<asio::ip::tcp::socket>(*io_context_);
    
    acceptor_->async_accept(*socket,
        [this, socket](const asio::error_code& error) {
            handle_accept(socket, error);
        });
}

void RouterMetricsEndpoint::handle_accept(std::shared_ptr<asio::ip::tcp::socket> socket,
                                         const asio::error_code& error) {
    if (!running_) {
        return;
    }
    
    if (!error) {
        HttpRequestHandler::handle_request(*socket, aggregator_);
    }
    
    start_accept();
}

} // namespace logpipeline
