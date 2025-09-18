#include "subscriber.hpp"
#include <zmq_addon.hpp>
#include <iostream>
#include <chrono>
#include <sstream>

namespace messenger {

SubscriberBus::SubscriberBus(const BusConfig& config, 
                           const std::vector<std::string>& topics,
                           MessageHandler handler)
    : config_(config)
    , topics_(topics)
    , handler_(std::move(handler))
    , context_(config.io_threads)
    , worker_pool_(config.worker_threads)
    , metrics_(config.metrics_period) {
}

SubscriberBus::~SubscriberBus() {
    stop();
}

void SubscriberBus::start() {
    if (running_.load(std::memory_order_relaxed)) {
        return;
    }
    
    try {
        sub_socket_ = std::make_unique<zmq::socket_t>(context_, zmq::socket_type::sub);
        sub_socket_->set(zmq::sockopt::rcvhwm, config_.hwm);
        
        sub_socket_->connect(config_.sub_connect_addr);
        
        for (const auto& topic : topics_) {
            sub_socket_->set(zmq::sockopt::subscribe, topic);
        }
        
        running_.store(true, std::memory_order_relaxed);
        start_time_ = std::chrono::steady_clock::now();
        io_thread_ = std::thread(&SubscriberBus::io_thread_loop, this);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to start SubscriberBus: " << e.what() << std::endl;
        throw;
    }
}

void SubscriberBus::stop() {
    if (!running_.load(std::memory_order_relaxed)) {
        return;
    }
    
    running_.store(false, std::memory_order_relaxed);
    
    if (io_thread_.joinable()) {
        io_thread_.join();
    }
    
    worker_pool_.stop();
    worker_pool_.join();
    
    sub_socket_.reset();
}

void SubscriberBus::io_thread_loop() {
    try {
        while (running_.load(std::memory_order_relaxed)) {
            std::vector<zmq::message_t> messages;
            auto result = zmq::recv_multipart(*sub_socket_, std::back_inserter(messages), 
                                            zmq::recv_flags::dontwait);
            
            if (result.has_value() && messages.size() >= 2) {
                std::string topic(static_cast<char*>(messages[0].data()), messages[0].size());
                std::string payload(static_cast<char*>(messages[1].data()), messages[1].size());
                
                Message message(std::move(topic), std::move(payload));
                
                boost::asio::post(worker_pool_, [this, message = std::move(message)]() {
                    process_message(message);
                });
                
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "I/O thread error: " << e.what() << std::endl;
    }
}

void SubscriberBus::process_message(const Message& message) {
    try {
        metrics_.record_message_processed();
        
        if (message.payload.size() >= 8) {
            uint64_t timestamp_ns;
            std::memcpy(&timestamp_ns, message.payload.data(), sizeof(timestamp_ns));
            
            auto now = std::chrono::steady_clock::now();
            auto message_time = std::chrono::steady_clock::time_point(
                std::chrono::nanoseconds(timestamp_ns));
            auto latency = now - message_time;
            
            metrics_.record_latency(latency);
        }
        
        if (handler_) {
            handler_(message);
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error processing message: " << e.what() << std::endl;
    }
}

} // namespace messenger
