#include "metrics.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace messenger {

Metrics::Metrics(std::chrono::milliseconds window_size)
    : window_size_(window_size)
    , last_rate_calc_(std::chrono::steady_clock::now()) {
}

void Metrics::record_latency(std::chrono::nanoseconds latency) {
    auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(samples_mutex_);
    latency_samples_.push_back({now, static_cast<double>(latency.count())});
    prune_old_samples_locked(now);
}

void Metrics::record_message_processed() {
    messages_processed_.fetch_add(1);
}

Metrics::Stats Metrics::get_stats() {
    Stats stats;
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_rate_calc_);
    uint64_t current_count = messages_processed_.load();
    
    if (elapsed.count() > 0) {
        double rate = (current_count - last_message_count_) * 1000.0 / elapsed.count();
        stats.messages_per_second = rate;
        last_message_count_ = current_count;
        last_rate_calc_ = now;
    }
    
    {
        std::lock_guard<std::mutex> lock(samples_mutex_);
        prune_old_samples_locked(now);
        if (!latency_samples_.empty()) {
            std::vector<double> sorted_samples;
            sorted_samples.reserve(latency_samples_.size());
            for (const auto& sample : latency_samples_) {
                sorted_samples.push_back(sample.latency_ns);
            }
            std::sort(sorted_samples.begin(), sorted_samples.end());
            
            stats.p50 = calculate_percentile(sorted_samples, 50.0);
            stats.p90 = calculate_percentile(sorted_samples, 90.0);
            stats.p99 = calculate_percentile(sorted_samples, 99.0);
        }
    }
    
    stats.messages_processed = current_count;
    
    return stats;
}

void Metrics::reset() {
    std::lock_guard<std::mutex> lock(samples_mutex_);
    latency_samples_.clear();
    messages_processed_.store(0);
    last_rate_calc_ = std::chrono::steady_clock::now();
    last_message_count_ = 0;
}

double Metrics::calculate_percentile(const std::vector<double>& sorted_samples, double percentile) {
    if (sorted_samples.empty()) return 0.0;
    
    double index = (percentile / 100.0) * (sorted_samples.size() - 1);
    size_t lower = static_cast<size_t>(std::floor(index));
    size_t upper = static_cast<size_t>(std::ceil(index));
    
    if (lower == upper) {
        return sorted_samples[lower];
    }
    
    double weight = index - lower;
    return sorted_samples[lower] * (1.0 - weight) + sorted_samples[upper] * weight;
}

void Metrics::prune_old_samples_locked(std::chrono::steady_clock::time_point now) {
    const auto cutoff = now - window_size_;
    while (!latency_samples_.empty() && latency_samples_.front().timestamp < cutoff) {
        latency_samples_.pop_front();
    }
}

namespace metrics_utils {

std::string format_stats(const Metrics::Stats& stats) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    oss << "p50=" << format_duration(std::chrono::nanoseconds(static_cast<int64_t>(stats.p50)))
        << " p90=" << format_duration(std::chrono::nanoseconds(static_cast<int64_t>(stats.p90)))
        << " p99=" << format_duration(std::chrono::nanoseconds(static_cast<int64_t>(stats.p99)))
        << " msgs/sec=" << stats.messages_per_second
        << " processed=" << stats.messages_processed;
    return oss.str();
}

std::string format_duration(std::chrono::nanoseconds duration) {
    auto ns = duration.count();
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);

    if (ns < 1000) {
        return std::to_string(ns) + "ns";
    } else if (ns < 1000000) {
        oss << (static_cast<double>(ns) / 1000.0) << "us";
        return oss.str();
    } else if (ns < 1000000000) {
        oss << (static_cast<double>(ns) / 1000000.0) << "ms";
        return oss.str();
    } else {
        oss << (static_cast<double>(ns) / 1000000000.0) << "s";
        return oss.str();
    }
}

} // namespace metrics_utils
} // namespace messenger
