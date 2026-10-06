#include "logger.hpp"
#include "circuit_breaker.hpp"


CircuitBreaker::CircuitBreaker(int failure_threshold, std::chrono::seconds open_duration)
    : state_(State::CLOSED),
      failure_count_(0),
      opened_at_(std::chrono::steady_clock::time_point{}),
      failure_threshold_(failure_threshold),
      open_duration_(open_duration) {}

bool CircuitBreaker::allowRequest() {
    if (state_.load() == State::OPEN) {
        auto now = std::chrono::steady_clock::now();
        State expected = State::OPEN;
        if (state_.compare_exchange_strong(expected, State::HALF_OPEN)) {
            LOG_INFO("Open duration elapsed — transitioning to HALF_OPEN, allowing one trial request.");
            return true;
        }
        return false;
    }
    return true;
}

void CircuitBreaker::recordSuccess() {
    State previous = state_.exchange(State::CLOSED);

    if (previous != State::CLOSED) {
        failure_count_.store(0);
        LOG_INFO("Recovered — transitioning to CLOSED.");
    }
}

void CircuitBreaker::recordFailure() {
    int count = ++failure_count_;

    if (state_.load() == State::HALF_OPEN) {
        state_.store(State::OPEN);
        opened_at_.store(std::chrono::steady_clock::now());
        LOG_WARN("Trial request failed in HALF_OPEN — reopening circuit for {}s.",
                     open_duration_.count());
        return;
    }

    if (count >= failure_threshold_ && state_.load() == State::CLOSED) {
        state_.store(State::OPEN);
        opened_at_.store(std::chrono::steady_clock::now());
        LOG_CRITICAL("Failure threshold reached ({} failures) — circuit OPEN for {}s.",
                         count, open_duration_.count());
    }
}

bool CircuitBreaker::isOpen() const {
    return state_.load() == State::OPEN;  // std::memory_order_relaxed
}

CircuitBreaker::State CircuitBreaker::getState() const {
    return state_.load();  // std::memory_order_acquire
}

int CircuitBreaker::getFailureCount() const {
    return failure_count_.load();  // std::memory_order_relaxed
}