#pragma once

#include <atomic>
#include <chrono>


class CircuitBreaker {
public:
    enum class State { CLOSED, OPEN, HALF_OPEN };

    explicit CircuitBreaker(int failure_threshold = 10,
                             std::chrono::seconds open_duration = std::chrono::seconds(30));

    bool allowRequest();

    void recordSuccess();

    void recordFailure();

    bool isOpen() const;
    State getState() const;
    int getFailureCount() const;

private:
    std::atomic<State> state_;
    std::atomic<int> failure_count_;
    std::atomic<std::chrono::steady_clock::time_point> opened_at_;

    const int failure_threshold_;
    const std::chrono::seconds open_duration_;
};