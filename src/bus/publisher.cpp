#include "publisher.hpp"
#include <zmq_addon.hpp>
#include <iostream>
#include <chrono>

namespace messenger {


PublisherBus::PublisherBus(const BusConfig& config)
    : config_(config)
    , context_(config.io_threads) {
}

PublisherBus::~PublisherBus() {
    stop();
}

void PublisherBus::start() {
    if (running_.load()) {
        return;
    }
    
    pull_socket_.reset(new zmq::socket_t(context_, zmq::socket_type::pull));
    pub_socket_.reset(new zmq::socket_t(context_, zmq::socket_type::pub));
    
    pull_socket_->set(zmq::sockopt::rcvhwm, config_.hwm);
    pub_socket_->set(zmq::sockopt::sndhwm, config_.hwm);
    
    pull_socket_->bind(config_.inproc_ingress);
    pub_socket_->bind(config_.pub_bind_addr);
    
    running_.store(true);
    io_thread_ = std::thread(&PublisherBus::io_thread_loop, this);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

void PublisherBus::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    
    if (io_thread_.joinable()) {
        io_thread_.join();
    }
    
    pull_socket_.reset();
    pub_socket_.reset();
}

void PublisherBus::produce(const Message& msg) {
    auto& push_socket = get_thread_local_push_socket();
    
    zmq::message_t topic_msg(msg.topic.size());
    std::memcpy(topic_msg.data(), msg.topic.data(), msg.topic.size());
    push_socket.send(topic_msg, zmq::send_flags::sndmore);
    
    zmq::message_t payload_msg(msg.payload.size());
    std::memcpy(payload_msg.data(), msg.payload.data(), msg.payload.size());
    push_socket.send(payload_msg, zmq::send_flags::none);
}

void PublisherBus::io_thread_loop() {
    while (running_.load()) {
        std::vector<zmq::message_t> msgs;
        auto result = zmq::recv_multipart(*pull_socket_, std::back_inserter(msgs), 
                                        zmq::recv_flags::dontwait);
        
        if (result.has_value() && msgs.size() >= 2) {
            pub_socket_->send(msgs[0], zmq::send_flags::sndmore);
            pub_socket_->send(msgs[1], zmq::send_flags::none);
        } else {
            // no message available, wait briefly
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    }
}

zmq::socket_t& PublisherBus::get_thread_local_push_socket() {
    std::lock_guard<std::mutex> lock(socket_mutex_);
    auto thread_id = std::this_thread::get_id();
    
    auto it = thread_sockets_.find(thread_id);
    if (it == thread_sockets_.end()) {
        auto socket = std::unique_ptr<zmq::socket_t>(new zmq::socket_t(context_, zmq::socket_type::push));
        socket->set(zmq::sockopt::sndhwm, config_.hwm);
        socket->connect(config_.inproc_ingress);
        thread_sockets_[thread_id] = std::move(socket);
        return *thread_sockets_[thread_id];
    }
    return *it->second;
}

} // namespace messenger