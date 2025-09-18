#pragma once

#include "types.hpp"
#include "metrics.hpp"
#include <zmq.hpp>
#include <zmq_addon.hpp>
#include <boost/asio.hpp>
#include <thread>
#include <atomic>
#include <memory>
#include <vector>

namespace messenger {

/**
 * SubscriberBus implements the subscriber side of the messaging bus.
 * 
 * Architecture:
 * - I/O thread: Owns SUB socket, receives messages, posts to worker pool
 * - Worker pool: Boost.Asio thread_pool for CPU-intensive message processing
 * - No heavy work in I/O thread to maintain low latency
 */
class SubscriberBus {
public:
    SubscriberBus(const BusConfig& config, 
                  const std::vector<std::string>& topics,
                  MessageHandler handler);
    ~SubscriberBus();
    
    // Start the I/O thread and worker pool
    void start();
    
    // Stop gracefully
    void stop();
    
    // Check if running
    bool is_running() const { return running_.load(std::memory_order_relaxed); }
    
    // Get metrics
    Metrics::Stats get_metrics() { return metrics_.get_stats(); }

private:
    // I/O thread main loop
    void io_thread_loop();
    
    // Worker function wrapper
    void process_message(const Message& message);
    
    BusConfig config_;
    std::vector<std::string> topics_;
    MessageHandler handler_;
    
    zmq::context_t context_;
    std::unique_ptr<zmq::socket_t> sub_socket_;
    
    // Threading
    std::atomic<bool> running_{false};
    std::thread io_thread_;
    boost::asio::thread_pool worker_pool_;
    
    // Metrics
    Metrics metrics_;
    
    // Start time for latency calculation
    std::chrono::steady_clock::time_point start_time_;
};

} // namespace messenger
