#pragma once

#include <vector>
#include <deque>
#include <chrono>
#include <atomic>
#include <mutex>
#include <string>

namespace messenger {

/**
 * Thread-safe metrics collector for latency and throughput statistics
 */
class Metrics {
public:
    struct Stats {
        double p50 = 0.0;
        double p90 = 0.0;
        double p99 = 0.0;
        uint64_t messages_processed = 0;
        double messages_per_second = 0.0;
    };

    explicit Metrics(std::chrono::milliseconds window_size = std::chrono::milliseconds(1000));
    
    void record_latency(std::chrono::nanoseconds latency);
    
    void record_message_processed();
    
    
    Stats get_stats();
    
    void reset();

private:
    double calculate_percentile(const std::vector<double>& sorted_samples, double percentile);
    void prune_old_samples_locked(std::chrono::steady_clock::time_point now);
    
    std::chrono::milliseconds window_size_;
    
    struct LatencySample {
        std::chrono::steady_clock::time_point timestamp;
        double latency_ns;
    };
    std::deque<LatencySample> latency_samples_;
    std::mutex samples_mutex_;
    
    std::atomic<uint64_t> messages_processed_{0};
    
    std::chrono::steady_clock::time_point last_rate_calc_;
    uint64_t last_message_count_{0};
};

/**
 * Utility functions for metrics formatting
 */
namespace metrics_utils {
    std::string format_stats(const Metrics::Stats& stats);
    std::string format_duration(std::chrono::nanoseconds duration);
}

} // namespace messenger
