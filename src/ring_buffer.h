#pragma once

#include <deque>
#include <mutex>

template <typename T>
class TimedRingBuffer {
public:
    void Push(const T& item) {
        std::scoped_lock lock(mutex_);
        buffer_.push_back(item);
        TrimLocked();
    }

    void Push(T&& item) {
        std::scoped_lock lock(mutex_);
        buffer_.push_back(std::move(item));
        TrimLocked();
    }

    void SetMaxDurationHns(long long durationHns) {
        std::scoped_lock lock(mutex_);
        maxDurationHns_ = durationHns;
        TrimLocked();
    }

    std::deque<T> Snapshot() const {
        std::scoped_lock lock(mutex_);
        return buffer_;
    }

    void Clear() {
        std::scoped_lock lock(mutex_);
        buffer_.clear();
    }

private:
    void TrimLocked() {
        if (buffer_.empty()) {
            return;
        }
        const auto latest = buffer_.back().timestampHns;
        while (!buffer_.empty() && latest - buffer_.front().timestampHns > maxDurationHns_) {
            buffer_.pop_front();
        }
    }

    mutable std::mutex mutex_;
    std::deque<T> buffer_;
    long long maxDurationHns_ = 30 * 10000 * 1000; // 30 seconds default
};
