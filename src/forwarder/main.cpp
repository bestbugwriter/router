#include "forwarder/tcp_server.h"
#include "forwarder/kafka_producer.h"
#include "forwarder/metrics_aggregator.h"
#include "forwarder/metrics_endpoint.h"
#include "common/config.h"
#include <spdlog/spdlog.h>
#include <iostream>
#include <csignal>
#include <atomic>

std::atomic<bool> g_shutdown_requested{false};

void signal_handler(int signal) {
    spdlog::info("Received signal {}, shutting down...", signal);
    g_shutdown_requested = true;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
        return 1;
    }
    
    // Setup signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    try {
        // Load configuration
        logpipeline::config::ForwarderConfig config;
        if (!config.load_from_file(argv[1])) {
            std::cerr << "Failed to load configuration from: " << argv[1] << std::endl;
            return 1;
        }
        
        spdlog::info("Starting log forwarder on {}:{}", config.listen_host, config.listen_port);
        
        // Initialize components
        auto kafka_producer = std::make_unique<logpipeline::KafkaProducer>(
            config.kafka_bootstrap_servers,
            config.kafka_default_props
        );
        
        if (!kafka_producer->start()) {
            spdlog::error("Failed to start Kafka producer");
            return 1;
        }
        
        auto metrics_aggregator = std::make_unique<logpipeline::MetricsAggregator>();
        
        auto tcp_server = std::make_unique<logpipeline::TCPServer>(
            config.listen_host,
            config.listen_port,
            config.io_threads,
            kafka_producer.get(),
            metrics_aggregator.get()
        );
        
        if (!tcp_server->start()) {
            spdlog::error("Failed to start TCP server");
            return 1;
        }
        
        // Start metrics endpoint
        auto metrics_endpoint = std::make_unique<logpipeline::MetricsEndpoint>(
            config.metrics_prometheus_listen,
            metrics_aggregator.get()
        );
        
        if (!metrics_endpoint->start()) {
            spdlog::error("Failed to start metrics endpoint");
            return 1;
        }
        
        spdlog::info("Log forwarder started successfully");
        spdlog::info("Metrics endpoint available at: http://{}", config.metrics_prometheus_listen);
        
        // Main loop
        while (!g_shutdown_requested) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // Update metrics
            metrics_aggregator->set_active_connections(tcp_server->get_active_connections());
            
            // Update Kafka metrics
            static uint64_t last_kafka_sent = 0;
            static uint64_t last_kafka_failed = 0;
            
            uint64_t current_sent = kafka_producer->get_messages_sent();
            uint64_t current_failed = kafka_producer->get_messages_failed();
            
            if (current_sent > last_kafka_sent) {
                metrics_aggregator->increment_kafka_messages_sent("logs");
                last_kafka_sent = current_sent;
            }
            
            if (current_failed > last_kafka_failed) {
                metrics_aggregator->increment_kafka_messages_failed("logs");
                last_kafka_failed = current_failed;
            }
        }
        
        // Graceful shutdown
        spdlog::info("Shutting down log forwarder...");
        
        metrics_endpoint->stop();
        tcp_server->stop();
        kafka_producer->stop();
        
        spdlog::info("Log forwarder stopped");
        
    } catch (const std::exception& e) {
        spdlog::error("Fatal error: {}", e.what());
        return 1;
    }
    
    return 0;
}
