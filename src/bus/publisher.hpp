#pragma once

#include "types.hpp"
#include <zmq.hpp>
#include <zmq_addon.hpp>
#include <thread>
#include <atomic>
#include <memory>

namespace messenger {

/**
 * PublisherBus implements the publisher side of the messaging bus.
 * 
 * Architecture:
 * - Producer threads: Each has a thread-local PUSH socket connected to inproc://ingress
 * - I/O thread: Owns PULL socket (bound to inproc://ingress) and PUB socket (bound to TCP)
 * - No socket sharing across threads (ZeroMQ sockets are not thread-safe)
 */
class PublisherBus {
public:
    explicit PublisherBus(const BusConfig& config = BusConfig{});
    ~PublisherBus();
    
    // Start the I/O thread and warm up
    void start();
    
    // Stop the I/O thread gracefully
    void stop();
    
    // Thread-safe message publishing
    // Creates thread-local PUSH socket on first call
    void produce(const Message& message);
    
    // Check if the bus is running
    bool is_running() const { return running_.load(std::memory_order_relaxed); }

private:
    // I/O thread main loop
    void io_thread_loop();
    
    // Create thread-local PUSH socket
    zmq::socket_t& get_thread_local_push_socket();
    
    BusConfig config_;
    zmq::context_t context_;
    
    // I/O thread owns these sockets
    std::unique_ptr<zmq::socket_t> pull_socket_;
    std::unique_ptr<zmq::socket_t> pub_socket_;
    
    // Threading
    std::atomic<bool> running_{false};
    std::thread io_thread_;
    
    // Thread-local storage for PUSH sockets
    static thread_local std::unique_ptr<zmq::socket_t> thread_local_push_;
    static thread_local bool thread_local_initialized_;
};

} // namespace messenger
