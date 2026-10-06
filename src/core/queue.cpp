#include "queue.hpp"
#include "meta.hpp"


template <typename T>
bool Queue<T>::push(T value) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (closed_) return false;
        if (queue_.size() >= max_capacity_) {
            queue_.pop();
        }
        queue_.push(std::move(value));
    }
    cond_var_.notify_one();
    return true;
}

template <typename T>
bool Queue<T>::pop(T& out) {
    std::unique_lock<std::mutex> lock(mutex_);
    cond_var_.wait(lock, [this] { return !queue_.empty() || closed_; });
    if (queue_.empty()) return false;

    out = std::move(queue_.front());
    queue_.pop();
    return true;
}

template <typename T>
bool Queue<T>::try_pop(T& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty()) return false;
    value = std::move(queue_.front());
    queue_.pop();
    return true;
}

template <typename T>
size_t Queue<T>::pop_batch(std::vector<T>& out,
                           size_t max_batch,
                           std::chrono::milliseconds timeout) {
    out.clear();
    if (max_batch == 0) return 0;

    std::unique_lock<std::mutex> lock(mutex_);

    cond_var_.wait(lock, [this] { return !queue_.empty() || closed_; });
    if (queue_.empty()) return 0;

    if (queue_.size() < max_batch && timeout.count() > 0) {
        cond_var_.wait_for(lock, timeout, [&] {
            return queue_.size() >= max_batch || closed_;
        });
    }

    const size_t n = std::min(max_batch, queue_.size());
    out.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        out.push_back(std::move(queue_.front()));
        queue_.pop();
    }
    return n;
}

template <typename T>
size_t Queue<T>::try_pop_batch(std::vector<T>& out, size_t max_batch) {
    out.clear();
    std::lock_guard<std::mutex> lock(mutex_);
    const size_t n = std::min(max_batch, queue_.size());
    out.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        out.push_back(std::move(queue_.front()));
        queue_.pop();
    }
    return n;
}

template <typename T>
void Queue<T>::close() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        closed_ = true;
    }
    cond_var_.notify_all();
}

template <typename T>
bool Queue<T>::closed() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return closed_;
}

template <typename T>
size_t Queue<T>::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

template <typename T>
bool Queue<T>::empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
}

template class Queue<int>;
template class Queue<std::string>;
template class Queue<FrameMeta>;
template class Queue<SourceMeta>;