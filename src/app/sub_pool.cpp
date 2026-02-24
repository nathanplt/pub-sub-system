#include "bus/subscriber.hpp"
#include "bus/types.hpp"
#include "bus/metrics.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <signal.h>

using namespace messenger;

std::atomic<bool> subscribers_running{true};

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        subscribers_running.store(false);
    }
}

void simulate_handler_work() {
    // simulate some CPU work (0.5-1ms)
    int total = 0;
    for (int i = 0; i < 10000; ++i) {
        total += i * i;
    }
}

void metrics_thread(SubscriberBus& bus) {
    while (subscribers_running.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        auto stats = bus.get_metrics();
        std::cout << "METRICS: " << metrics_utils::format_stats(stats) << std::endl;
    }
}

int main(int argc, char* argv[]) {
    std::string sub_addr = "tcp://127.0.0.1:5556";
    int num_workers = 4;
    int hwm = 10000;
    bool simulate_work = true;
    std::vector<std::string> topics = {"topic0", "topic1", "topic2", "topic3"};
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--sub") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --sub" << std::endl;
                return 1;
            }
            sub_addr = argv[i + 1];
            ++i;
        } 
        else if (arg == "--workers") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --workers" << std::endl;
                return 1;
            }
            num_workers = std::atoi(argv[i + 1]);
            ++i;
        } 
        else if (arg == "--hwm") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --hwm" << std::endl;
                return 1;
            }
            hwm = std::atoi(argv[i + 1]);
            ++i;
        }
        else if (arg == "--topics") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --topics" << std::endl;
                return 1;
            }
            std::string topics_str = argv[i + 1];
            topics.clear();
            size_t pos = 0;
            while (pos < topics_str.length()) {
                size_t next_pos = topics_str.find(',', pos);
                if (next_pos == std::string::npos) {
                    topics.push_back(topics_str.substr(pos));
                    break;
                } 
                else {
                    topics.push_back(topics_str.substr(pos, next_pos - pos));
                    pos = next_pos + 1;
                }
            }
            ++i;
        }
        else if (arg == "--no-work") {
            simulate_work = false;
        }
    }
    
    std::cout << "Starting subscriber with worker pool:" << std::endl;
    std::cout << "  Subscriber address: " << sub_addr << std::endl;
    std::cout << "  Worker threads: " << num_workers << std::endl;
    std::cout << "  HWM: " << hwm << std::endl;
    std::cout << "  Simulate work: " << (simulate_work ? "yes" : "no") << std::endl;
    std::cout << "  Topics: ";
    for (const auto& topic : topics) {
        std::cout << topic << " ";
    }
    std::cout << std::endl << std::endl;
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    BusConfig config;
    config.sub_connect_addr = sub_addr;
    config.worker_threads = num_workers;
    config.hwm = hwm;
    config.metrics_period = std::chrono::milliseconds(1000);
    
    MessageHandler handler = [simulate_work](const Message& msg) {
        if (!simulate_work) {
            return;
        }
        simulate_handler_work();
    };

    SubscriberBus bus(config, topics, handler);
    bus.start();
    
    std::cout << "Subscriber started. Waiting for messages..." << std::endl;
    std::cout << "Press Ctrl+C to stop." << std::endl << std::endl;
    
    std::thread metrics_worker(metrics_thread, std::ref(bus));
    
    while (subscribers_running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << std::endl << "Shutting down..." << std::endl;
    
    metrics_worker.join();
    
    bus.stop();
    
    auto final_stats = bus.get_metrics();
    std::cout << "FINAL METRICS: " << metrics_utils::format_stats(final_stats) << std::endl;
    
    std::cout << "Subscriber stopped" << std::endl;
    
    return 0;
}
