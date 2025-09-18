#include "bus/subscriber.hpp"
#include "bus/types.hpp"
#include "bus/metrics.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <signal.h>

using namespace messenger;

std::atomic<bool> g_running{true};

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        g_running.store(false);
    }
}

void message_handler(const Message& message) {
    // simulate some CPU work (0.5-1ms)
    auto start = std::chrono::steady_clock::now();
    
    if (message.payload.size() >= 8) {
        uint64_t timestamp_ns;
        std::memcpy(&timestamp_ns, message.payload.data(), sizeof(timestamp_ns));
        
        std::string data;
        if (message.payload.size() > 8) {
            data = std::string(message.payload.data() + 8, message.payload.size() - 8);
        }
        
        volatile int sum = 0;
        for (int i = 0; i < 10000; ++i) {
            sum += i * i;
        }
        
        auto end = std::chrono::steady_clock::now();
        auto processing_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        static std::atomic<int> message_count{0};
        int count = message_count.fetch_add(1) + 1;
        
        if (count % 1000 == 0) {
            std::cout << "Processed " << count << " messages. "
                      << "Topic: " << message.topic 
                      << ", Data: " << data.substr(0, 20) << "..."
                      << ", Processing time: " << processing_time.count() << "μs"
                      << std::endl;
        }
    }
}

void metrics_thread(SubscriberBus& bus) {
    while (g_running.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        auto stats = bus.get_metrics();
        std::cout << "METRICS: " << metrics_utils::format_stats(stats) << std::endl;
    }
}

int main(int argc, char* argv[]) {
    std::string sub_addr = "tcp://127.0.0.1:5556";
    int num_workers = 4;
    std::vector<std::string> topics = {"topic0", "topic1", "topic2", "topic3"};
    
    for (int i = 1; i < argc; i += 2) {
        if (i + 1 >= argc) break;
        
        std::string arg = argv[i];
        if (arg == "--sub" && i + 1 < argc) {
            sub_addr = argv[i + 1];
        } else if (arg == "--workers" && i + 1 < argc) {
            num_workers = std::atoi(argv[i + 1]);
        } else if (arg == "--topics" && i + 1 < argc) {
            std::string topics_str = argv[i + 1];
            topics.clear();
            size_t pos = 0;
            while (pos < topics_str.length()) {
                size_t next_pos = topics_str.find(',', pos);
                if (next_pos == std::string::npos) {
                    topics.push_back(topics_str.substr(pos));
                    break;
                } else {
                    topics.push_back(topics_str.substr(pos, next_pos - pos));
                    pos = next_pos + 1;
                }
            }
        }
    }
    
    std::cout << "Starting subscriber with worker pool:" << std::endl;
    std::cout << "  Subscriber address: " << sub_addr << std::endl;
    std::cout << "  Worker threads: " << num_workers << std::endl;
    std::cout << "  Topics: ";
    for (const auto& topic : topics) {
        std::cout << topic << " ";
    }
    std::cout << std::endl << std::endl;
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    try {
        BusConfig config;
        config.sub_connect_addr = sub_addr;
        config.worker_threads = num_workers;
        config.hwm = 10000;
        config.metrics_period = std::chrono::milliseconds(1000);
        
        SubscriberBus bus(config, topics, message_handler);
        bus.start();
        
        std::cout << "Subscriber started. Waiting for messages..." << std::endl;
        std::cout << "Press Ctrl+C to stop." << std::endl << std::endl;
        
        std::thread metrics_worker(metrics_thread, std::ref(bus));
        
        while (g_running.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        std::cout << std::endl << "Shutting down..." << std::endl;
        
        metrics_worker.join();
        
        bus.stop();
        
        auto final_stats = bus.get_metrics();
        std::cout << "FINAL METRICS: " << metrics_utils::format_stats(final_stats) << std::endl;
        
        std::cout << "Subscriber stopped." << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
