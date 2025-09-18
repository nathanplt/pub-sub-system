#include "publisher.hpp"
#include <zmq_addon.hpp>
#include <iostream>
#include <chrono>

namespace messenger {

// Thread-local storage definitions
thread_local std::unique_ptr<zmq::socket_t> PublisherBus::thread_local_push_ = nullptr;
thread_local bool PublisherBus::thread_local_initialized_ = false;

PublisherBus::PublisherBus(const BusConfig& config)
    : config_(config)
    , context_(config.io_threads) {
}

PublisherBus::~PublisherBus() {
    stop();
}

void PublisherBus::start() {
    if (running_.load(std::memory_order_relaxed)) {
        return;
    }
    
    try {
        // Create and configure sockets
        pull_socket_ = std::make_unique<zmq::socket_t>(context_, zmq::socket_type::pull);
        pub_socket_ = std::make_unique<zmq::socket_t>(context_, zmq::socket_type::pub);
        
        // Set socket options
        pull_socket_->set(zmq::sockopt::rcvhwm, config_.hwm);
        pub_socket_->set(zmq::sockopt::sndhwm, config_.hwm);
        
        // Bind sockets
        pull_socket_->bind(config_.inproc_ingress);
        pub_socket_->bind(config_.pub_bind_addr);
        
        // Start I/O thread
        running_.store(true, std::memory_order_relaxed);
        io_thread_ = std::thread(&PublisherBus::io_thread_loop, this);
        
        // Warmup period to mitigate "slow joiner" problem
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to start PublisherBus: " << e.what() << std::endl;
        throw;
    }
}

void PublisherBus::stop() {
    if (!running_.load(std::memory_order_relaxed)) {
        return;
    }
    
    running_.store(false, std::memory_order_relaxed);
    
    if (io_thread_.joinable()) {
        io_thread_.join();
    }
    
    // Clean up sockets
    pull_socket_.reset();
    pub_socket_.reset();
}

void PublisherBus::produce(const Message& message) {
    auto& push_socket = get_thread_local_push_socket();
    
    try {
        // Send topic frame
        zmq::message_t topic_msg(message.topic.size());
        std::memcpy(topic_msg.data(), message.topic.data(), message.topic.size());
        push_socket.send(topic_msg, zmq::send_flags::sndmore);
        
        // Send payload frame
        zmq::message_t payload_msg(message.payload.size());
        std::memcpy(payload_msg.data(), message.payload.data(), message.payload.size());
        push_socket.send(payload_msg, zmq::send_flags::none);
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to send message: " << e.what() << std::endl;
    }
}

void PublisherBus::io_thread_loop() {
    try {
        while (running_.load(std::memory_order_relaxed)) {
            // Receive from PULL socket (inproc fan-in)
            std::vector<zmq::message_t> messages;
            auto result = zmq::recv_multipart(*pull_socket_, std::back_inserter(messages), 
                                            zmq::recv_flags::dontwait);
            
            if (result.has_value() && messages.size() >= 2) {
                // Forward to PUB socket
                // Send topic frame
                pub_socket_->send(messages[0], zmq::send_flags::sndmore);
                
                // Send payload frame
                pub_socket_->send(messages[1], zmq::send_flags::none);
            } else {
                // No message available, yield briefly
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "I/O thread error: " << e.what() << std::endl;
    }
}

zmq::socket_t& PublisherBus::get_thread_local_push_socket() {
    if (!thread_local_initialized_) {
        thread_local_push_ = std::make_unique<zmq::socket_t>(context_, zmq::socket_type::push);
        thread_local_push_->set(zmq::sockopt::sndhwm, config_.hwm);
        thread_local_push_->connect(config_.inproc_ingress);
        thread_local_initialized_ = true;
    }
    return *thread_local_push_;
}

} // namespace messenger
