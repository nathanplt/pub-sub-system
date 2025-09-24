#include "subscriber.hpp"
#include <zmq_addon.hpp>
#include <iostream>
#include <chrono>
#include <sstream>

namespace messenger {

SubscriberBus::SubscriberBus(const BusConfig& config, const std::vector<std::string>& topics, MessageHandler handler)
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
    if (running_.load()) {
        return;
    }
    
    sub_socket_.reset(new zmq::socket_t(context_, zmq::socket_type::sub));
    sub_socket_->set(zmq::sockopt::rcvhwm, config_.hwm);
    
    sub_socket_->connect(config_.sub_connect_addr);
    
    for (const auto& topic : topics_) {
        sub_socket_->set(zmq::sockopt::subscribe, topic);
    }
    
    running_.store(true);
    start_time_ = std::chrono::steady_clock::now();
    io_thread_ = std::thread(&SubscriberBus::io_thread_loop, this);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

void SubscriberBus::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    
    if (io_thread_.joinable()) {
        io_thread_.join();
    }
    
    worker_pool_.stop();
    worker_pool_.join();
    
    sub_socket_.reset();
}

void SubscriberBus::io_thread_loop() {
    while (running_.load()) {
        std::vector<zmq::message_t> msgs;
        auto result = zmq::recv_multipart(*sub_socket_, std::back_inserter(msgs), zmq::recv_flags::dontwait);
        
        if (result.has_value() && msgs.size() >= 2) {
            std::string topic(static_cast<char*>(msgs[0].data()), msgs[0].size());
            std::string payload(static_cast<char*>(msgs[1].data()), msgs[1].size());
            
            Message msg(topic, payload);
            
            boost::asio::post(worker_pool_, [this, msg]() {
                process_message(msg);
            });
            
        } else {
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    }
}

void SubscriberBus::process_message(const Message& msg) {
    metrics_.record_message_processed();
    
    if (msg.payload.size() >= 8) {
        uint64_t ts;
        std::memcpy(&ts, msg.payload.data(), sizeof(ts));
        
        auto now = std::chrono::steady_clock::now();
        auto msg_time = std::chrono::steady_clock::time_point(std::chrono::nanoseconds(ts));
        auto latency = now - msg_time;
        
        metrics_.record_latency(latency);
    }
    
    if (handler_) {
        handler_(msg);
    }
}

} // namespace messenger