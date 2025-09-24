#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <cstdint>
#include <functional>

namespace messenger {

struct Message {
    std::string topic;
    std::string payload;
    
    Message(std::string topic, std::string payload) 
        : topic(topic), payload(payload) {}
};

struct BusConfig {
    std::string pub_bind_addr = "tcp://*:5556";
    std::string sub_connect_addr = "tcp://127.0.0.1:5556";
    std::string inproc_ingress = "inproc://ingress";
    
    int io_threads = 1;
    int worker_threads = 4;
    
    size_t max_queue = 10000;
    
    std::chrono::milliseconds metrics_period{1000};
    
    int hwm = 1000;
};

using MessageHandler = std::function<void(const Message&)>;

} // namespace messenger