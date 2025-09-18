#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <cstdint>
#include <functional>

namespace messenger {

/**
 * Message structure for the pub/sub bus.
 * Uses two-frame ZeroMQ multipart format: [topic][payload]
 */
struct Message {
    std::string topic;
    std::string payload;
    
    Message() = default;
    Message(std::string topic, std::string payload) 
        : topic(std::move(topic)), payload(std::move(payload)) {}
    
    // Move constructor and assignment
    Message(Message&&) = default;
    Message& operator=(Message&&) = default;
    
    // Copy constructor and assignment
    Message(const Message&) = default;
    Message& operator=(const Message&) = default;
};

/**
 * Configuration for the pub/sub bus
 */
struct BusConfig {
    // Network endpoints
    std::string pub_bind_addr = "tcp://*:5556";
    std::string sub_connect_addr = "tcp://127.0.0.1:5556";
    std::string inproc_ingress = "inproc://ingress";
    
    // Threading configuration
    int io_threads = 1;                    // ZeroMQ context I/O threads
    int worker_threads = 4;                // subscriber compute threads
    
    // Queue configuration
    size_t max_queue = 10000;              // optional bounded queue (0 = unbounded)
    
    // Metrics configuration
    std::chrono::milliseconds metrics_period{1000};
    
    // ZeroMQ socket options
    int hwm = 1000;                        // High Water Mark for all sockets
};

/**
 * Handler function type for message processing
 */
using MessageHandler = std::function<void(const Message&)>;

} // namespace messenger
