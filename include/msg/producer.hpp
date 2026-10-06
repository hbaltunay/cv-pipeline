#pragma once

#include <string>
#include <memory>
#include <librdkafka/rdkafkacpp.h>


class KafkaProducer {
private:
    std::string brokers_;
    std::string topic_;

    std::unique_ptr<RdKafka::Producer> producer_;
    std::unique_ptr<RdKafka::Conf> conf_;

public:
    KafkaProducer(const std::string& brokers, const std::string& topic);
    ~KafkaProducer();

    KafkaProducer(const KafkaProducer&) = delete;
    KafkaProducer& operator=(const KafkaProducer&) = delete;

    bool start();
    void stop();

    bool push(const std::string& key, const std::string& payload);
};