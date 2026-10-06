#pragma once

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>
#include <vector>


template <typename T>
class Queue {
private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cond_var_;
    size_t max_capacity_;
    bool closed_ = false;

public:
    explicit Queue(size_t max_capacity = 30)
        : max_capacity_(std::max<size_t>(1, max_capacity)) {}

    Queue(const Queue<T>&) = delete;
    Queue<T>& operator=(const Queue<T>&) = delete;

    bool push(T value);

    bool pop(T& out);

    bool try_pop(T& value);

    size_t pop_batch(std::vector<T>& out,
                     size_t max_batch,
                     std::chrono::milliseconds timeout = std::chrono::milliseconds(20));

    size_t try_pop_batch(std::vector<T>& out, size_t max_batch);

    void close();
    bool closed() const;

    size_t size() const;
    bool empty() const;
};