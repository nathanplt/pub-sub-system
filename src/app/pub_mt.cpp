#include "bus/publisher.hpp"
#include "bus/types.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include <cstring>

using namespace messenger;

void producer_thread(PublisherBus& bus, int tid, int msg_count, const std::string& topic_prefix) {
    for (int i = 0; i < msg_count; ++i) {
        auto now = std::chrono::steady_clock::now();
        auto ts = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
        
        std::string data = "Thread " + std::to_string(tid) + " Message " + std::to_string(i);
        std::string payload = std::to_string(ts) + "|" + data;
        
        std::string topic = topic_prefix + std::to_string(tid % 4);
        Message msg(topic, payload);
        
        bus.produce(msg);
    }
}

int main(int argc, char* argv[]) {
    std::string pub_addr = "tcp://*:5556";
    int num_producers = 4;
    int messages_per_producer = 10000;
    std::string topic_prefix = "topic";
    
    for (int i = 1; i < argc; i += 2) {
        if (i + 1 >= argc) break;
        
        std::string arg = argv[i];
        if (arg == "--pub" && i + 1 < argc) {
            pub_addr = argv[i + 1];
        } 
        else if (arg == "--producers" && i + 1 < argc) {
            num_producers = std::atoi(argv[i + 1]);
        } 
        else if (arg == "--messages" && i + 1 < argc) {
            messages_per_producer = std::atoi(argv[i + 1]);
        } 
        else if (arg == "--topics" && i + 1 < argc) {
            topic_prefix = argv[i + 1];
        }
    }
    
    std::cout << "Starting multithreaded publisher:" << std::endl;
    std::cout << "  Publishers: " << num_producers << std::endl;
    std::cout << "  Messages per producer: " << messages_per_producer << std::endl;
    std::cout << "  Total messages: " << (num_producers * messages_per_producer) << std::endl;
    std::cout << "  Publisher address: " << pub_addr << std::endl;
    std::cout << "  Topic prefix: " << topic_prefix << std::endl;
    std::cout << std::endl;
    
    BusConfig config;
    config.pub_bind_addr = pub_addr;
    config.worker_threads = 1; 
    config.hwm = 10000;
    
    PublisherBus bus(config);
    bus.start();
    
    std::cout << "Publisher started. Starting producer threads..." << std::endl;

    std::vector<std::thread> producers;
    auto start_time = std::chrono::steady_clock::now();
    
    for (int i = 0; i < num_producers; ++i) {
        producers.emplace_back(producer_thread, std::ref(bus), i, messages_per_producer, topic_prefix);
    }
    
    for (auto& producer : producers) {
        producer.join();
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    std::cout << "All messages sent in " << duration.count() << " ms" << std::endl;
    if (duration.count() > 0) {
        std::cout << "Rate: " << (num_producers * messages_per_producer * 1000.0 / duration.count()) 
                  << " messages/sec" << std::endl;
    } else {
        auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        std::cout << "Rate: " << (num_producers * messages_per_producer * 1000000.0 / duration_us.count()) 
                  << " messages/sec" << std::endl;
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    bus.stop();
    std::cout << "Publisher stopped" << std::endl;
    
    return 0;
}