#include "forwarder/metrics_endpoint.h"
#include "forwarder/metrics_aggregator.h"
#include <spdlog/spdlog.h>
#include <sstream>

namespace logpipeline {

class MetricsEndpoint::HttpRequestHandler {
public:
    explicit HttpRequestHandler(MetricsAggregator* aggregator) : aggregator_(aggregator) {}
    
    void handle_request(asio::ip::tcp::socket& socket) {
        try {
            // Read HTTP request
            asio::streambuf buffer;
            asio::read_until(socket, buffer, "\r\n\r\n");
            
            std::istream request_stream(&buffer);
            std::string request_line;
            std::getline(request_stream, request_line);
            
            // Parse request line
            std::istringstream request_stream_parser(request_line);
            std::string method, path, version;
            request_stream_parser >> method >> path >> version;
            
            // Only handle GET /metrics
            if (method == "GET" && path == "/metrics") {
                std::string metrics_data = aggregator_->get_metrics_text();
                
                // Send HTTP response
                std::ostringstream response;
                response << "HTTP/1.1 200 OK\r\n";
                response << "Content-Type: text/plain; version=0.0.4; charset=utf-8\r\n";
                response << "Content-Length: " << metrics_data.length() << "\r\n";
                response << "Connection: close\r\n";
                response << "\r\n";
                response << metrics_data;
                
                std::string response_str = response.str();
                asio::write(socket, asio::buffer(response_str));
                
                spdlog::debug("Served metrics request");
            } else {
                // Send 404
                std::string not_found = 
                    "HTTP/1.1 404 Not Found\r\n"
                    "Content-Length: 0\r\n"
                    "Connection: close\r\n"
                    "\r\n";
                
                asio::write(socket, asio::buffer(not_found));
            }
            
        } catch (const std::exception& e) {
            spdlog::error("Error handling HTTP request: {}", e.what());
        }
    }
    
private:
    MetricsAggregator* aggregator_;
};

MetricsEndpoint::MetricsEndpoint(const std::string& listen_address, MetricsAggregator* aggregator)
    : listen_address_(listen_address), aggregator_(aggregator), running_(false) {
    
    // Parse address
    size_t colon_pos = listen_address.find(':');
    if (colon_pos == std::string::npos) {
        host_ = listen_address;
        port_ = 9101; // Default Prometheus port
    } else {
        host_ = listen_address.substr(0, colon_pos);
        port_ = std::stoi(listen_address.substr(colon_pos + 1));
    }
}

MetricsEndpoint::~MetricsEndpoint() {
    stop();
}

bool MetricsEndpoint::start() {
    if (running_) {
        return true;
    }
    
    try {
        io_context_ = std::make_unique<asio::io_context>();
        
        // Create acceptor
        asio::ip::tcp::endpoint endpoint(asio::ip::make_address(host_), port_);
        acceptor_ = std::make_unique<asio::ip::tcp::acceptor>(*io_context_, endpoint);
        
        running_ = true;
        
        // Start IO thread
        io_thread_ = std::thread([this]() {
            try {
                io_context_->run();
            } catch (const std::exception& e) {
                spdlog::error("Metrics endpoint IO thread error: {}", e.what());
            }
        });
        
        // Start accepting connections
        start_accept();
        
        spdlog::info("Metrics endpoint started on {}:{}", host_, port_);
        return true;
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to start metrics endpoint: {}", e.what());
        return false;
    }
}

void MetricsEndpoint::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    if (io_context_) {
        io_context_->stop();
    }
    
    if (acceptor_ && acceptor_->is_open()) {
        asio::error_code ec;
        acceptor_->close(ec);
    }
    
    if (io_thread_.joinable()) {
        io_thread_.join();
    }
    
    spdlog::info("Metrics endpoint stopped");
}

void MetricsEndpoint::start_accept() {
    if (!running_) {
        return;
    }
    
    auto socket = std::make_shared<asio::ip::tcp::socket>(*io_context_);
    
    acceptor_->async_accept(*socket,
        [this, socket](const asio::error_code& error) {
            if (!error) {
                spdlog::debug("Metrics endpoint connection from: {}", 
                             socket->remote_endpoint().address().to_string());
                
                // Handle request in a separate thread or asynchronously
                auto handler = std::make_unique<HttpRequestHandler>(aggregator_);
                
                // For simplicity, handle synchronously
                // In production, you'd want to handle this asynchronously
                try {
                    handler->handle_request(*socket);
                } catch (const std::exception& e) {
                    spdlog::error("Error handling metrics request: {}", e.what());
                }
            } else {
                spdlog::error("Metrics endpoint accept error: {}", error.message());
            }
            
            // Continue accepting
            start_accept();
        });
}

void MetricsEndpoint::handle_accept(std::shared_ptr<asio::ip::tcp::socket> socket,
                                    const asio::error_code& error) {
    if (!error) {
        spdlog::debug("Metrics endpoint connection from: {}", 
                     socket->remote_endpoint().address().to_string());
        
        // Handle request
        auto handler = std::make_unique<HttpRequestHandler>(aggregator_);
        
        // Handle request asynchronously
        std::thread([socket = std::move(socket), handler = std::move(handler)]() {
            try {
                handler->handle_request(*socket);
            } catch (const std::exception& e) {
                spdlog::error("Error handling metrics request: {}", e.what());
            }
        }).detach();
    } else {
        spdlog::error("Metrics endpoint accept error: {}", error.message());
    }
    
    // Continue accepting
    start_accept();
}

} // namespace logpipeline
