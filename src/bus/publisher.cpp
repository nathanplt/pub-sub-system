#include "publisher.hpp"
#include <zmq_addon.hpp>
#include <iostream>
#include <chrono>
#include <unordered_map>

namespace messenger {

namespace {
struct PushSocketCacheEntry {
    uint64_t bus_token = 0;
    uint64_t owner_id = 0;
    zmq::socket_t* socket = nullptr;
};

thread_local std::unordered_map<const PublisherBus*, PushSocketCacheEntry> g_push_socket_cache;
std::atomic<uint64_t> g_next_bus_cache_token{1};
}

PublisherBus::PublisherBus(const BusConfig& config)
    : config_(config)
    , cache_token_(g_next_bus_cache_token.fetch_add(1, std::memory_order_relaxed))
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
    
    accepting_producers_.store(true, std::memory_order_release);
    accepted_messages_.store(0, std::memory_order_relaxed);
    forwarded_messages_.store(0, std::memory_order_relaxed);
    active_produce_calls_.store(0, std::memory_order_relaxed);
    
    running_.store(true);
    io_thread_ = std::thread(&PublisherBus::io_thread_loop, this);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

void PublisherBus::stop() {
    if (!running_.load()) {
        return;
    }
    
    close_producers();
    wait_drained(std::chrono::milliseconds::max());
    
    running_.store(false);
    
    if (io_thread_.joinable()) {
        io_thread_.join();
    }
    
    pull_socket_.reset();
    pub_socket_.reset();
}

void PublisherBus::close_producers() {
    accepting_producers_.store(false, std::memory_order_release);
}

bool PublisherBus::wait_drained(std::chrono::milliseconds timeout) {
    auto is_drained = [this]() {
        const bool producers_closed = !accepting_producers_.load(std::memory_order_acquire);
        const uint64_t active_calls = active_produce_calls_.load(std::memory_order_acquire);
        const uint64_t accepted = accepted_messages_.load(std::memory_order_acquire);
        const uint64_t forwarded = forwarded_messages_.load(std::memory_order_acquire);

        return producers_closed && active_calls == 0 && forwarded >= accepted;
    };

    if (timeout == std::chrono::milliseconds::max()) {
        while (!is_drained()) {
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
        return true;
    }

    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (is_drained()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
    return is_drained();
}

bool PublisherBus::produce(const Message& msg) {
    if (!running_.load()) {
        return false;
    }
    
    if (!accepting_producers_.load(std::memory_order_acquire)) {
        return false;
    }

    active_produce_calls_.fetch_add(1, std::memory_order_acq_rel);
    if (!accepting_producers_.load(std::memory_order_acquire)) {
        active_produce_calls_.fetch_sub(1, std::memory_order_acq_rel);
        return false;
    }
    
    bool sent = false;
    
    auto& push_socket = get_thread_local_push_socket();
    try {
        zmq::message_t topic_msg(msg.topic.size());
        std::memcpy(topic_msg.data(), msg.topic.data(), msg.topic.size());
        push_socket.send(topic_msg, zmq::send_flags::sndmore);
        
        zmq::message_t payload_msg(msg.payload.size());
        std::memcpy(payload_msg.data(), msg.payload.data(), msg.payload.size());
        push_socket.send(payload_msg, zmq::send_flags::none);
        
        sent = true;
    } catch (const zmq::error_t&) {
        sent = false;
    }
    
    if (sent) {
        accepted_messages_.fetch_add(1, std::memory_order_release);
    }
    active_produce_calls_.fetch_sub(1, std::memory_order_acq_rel);
    
    return sent;
}

void PublisherBus::io_thread_loop() {
    while (running_.load()) {
        std::vector<zmq::message_t> msgs;
        auto result = zmq::recv_multipart(*pull_socket_, std::back_inserter(msgs), zmq::recv_flags::dontwait);
        
        if (result.has_value() && msgs.size() >= 2) {
            try {
                pub_socket_->send(msgs[0], zmq::send_flags::sndmore);
                pub_socket_->send(msgs[1], zmq::send_flags::none);
                forwarded_messages_.fetch_add(1, std::memory_order_release);
            } catch (const zmq::error_t&) {
                if (!running_.load()) {
                    break;
                }
            }
        } else {
            // no message available, wait briefly
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    }
}

zmq::socket_t& PublisherBus::get_thread_local_push_socket() {
    auto& cache = g_push_socket_cache[this];
    if (cache.bus_token == cache_token_ && cache.socket != nullptr) {
        return *cache.socket;
    }

    if (cache.bus_token != cache_token_) {
        cache.bus_token = cache_token_;
        cache.owner_id = next_socket_owner_id_.fetch_add(1, std::memory_order_relaxed);
        cache.socket = nullptr;
    }

    std::lock_guard<std::mutex> lock(socket_mutex_);

    auto it = thread_sockets_.find(cache.owner_id);
    if (it == thread_sockets_.end()) {
        auto socket = std::make_unique<zmq::socket_t>(context_, zmq::socket_type::push);
        socket->set(zmq::sockopt::sndhwm, config_.hwm);
        socket->connect(config_.inproc_ingress);

        auto [inserted, _] = thread_sockets_.emplace(cache.owner_id, std::move(socket));
        cache.socket = inserted->second.get();
        return *cache.socket;
    }

    cache.socket = it->second.get();
    return *cache.socket;
}

} // namespace messenger
