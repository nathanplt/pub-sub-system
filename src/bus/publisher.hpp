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
    
    void start();
    
    void stop();
    
    void produce(const Message& message);
    
    bool is_running() const { return running_.load(std::memory_order_relaxed); }

private:
    void io_thread_loop();
    
    zmq::socket_t& get_thread_local_push_socket();
    
    BusConfig config_;
    zmq::context_t context_;
    
    std::unique_ptr<zmq::socket_t> pull_socket_;
    std::unique_ptr<zmq::socket_t> pub_socket_;
    
    std::atomic<bool> running_{false};
    std::thread io_thread_;
    
    static thread_local std::unique_ptr<zmq::socket_t> thread_local_push_;
    static thread_local bool thread_local_initialized_;
};

} // namespace messenger