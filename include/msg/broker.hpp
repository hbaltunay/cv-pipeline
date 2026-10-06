#pragma once

#include <thread>
#include <atomic>

#include "base.hpp"
#include "queue.hpp"
#include "meta.hpp"
#include "producer.hpp"
#include "circuit_breaker.hpp"


class KafkaBroker : public Process {
private:
    Queue<FrameMeta>& input_queue_;

    std::thread broker_thread_;
    std::atomic<bool> running_{false};

    CircuitBreaker circuit_breaker_;
    KafkaProducer producer_;

    void run();
    void publish(const FrameMeta& meta);
    std::string frameMetaToJson(const FrameMeta& meta);

public:
    KafkaBroker(Queue<FrameMeta>& input_queue, const std::string& brokers, const std::string& topic);
    ~KafkaBroker();

    bool start() override;
    void stop() override;
};
