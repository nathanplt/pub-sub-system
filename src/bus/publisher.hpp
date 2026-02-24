#pragma once

#include "types.hpp"
#include <zmq.hpp>
#include <zmq_addon.hpp>
#include <thread>
#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>

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
    
    // Disallow new produce() calls from being accepted
    void close_producers();
    
    // Wait until all accepted messages have been forwarded by the I/O thread
    bool wait_drained(std::chrono::milliseconds timeout = std::chrono::milliseconds::max());
    
    // Returns false if producers are closed or the bus is not running
    bool produce(const Message& message);
    
    bool is_running() const { return running_.load(); }

private:
    void io_thread_loop();
    
    zmq::socket_t& get_thread_local_push_socket();
    
    BusConfig config_;
    zmq::context_t context_;
    
    std::unique_ptr<zmq::socket_t> pull_socket_;
    std::unique_ptr<zmq::socket_t> pub_socket_;
    
    std::atomic<bool> running_{false};
    std::thread io_thread_;

    // for thread-local sockets (per PublisherBus object)
    const uint64_t cache_token_;
    std::atomic<uint64_t> next_socket_owner_id_{1};
    std::mutex socket_mutex_;
    std::unordered_map<uint64_t, std::unique_ptr<zmq::socket_t>> thread_sockets_;
    
    // for correct stopping conditions
    std::atomic<bool> accepting_producers_{false};
    std::atomic<uint64_t> accepted_messages_{0};
    std::atomic<uint64_t> forwarded_messages_{0};
    std::atomic<uint64_t> active_produce_calls_{0};
};

} // namespace messenger
