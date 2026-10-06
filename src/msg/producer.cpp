#include "logger.hpp"
#include "producer.hpp"


KafkaProducer::KafkaProducer(const std::string& brokers, const std::string& topic)
    : brokers_(brokers), topic_(topic) {}

KafkaProducer::~KafkaProducer() {
    stop();
}

bool KafkaProducer::start() {
    std::string errstr;
    RdKafka::Conf* conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);
    if (!conf) {
        LOG_ERROR("Failed to create global configuration object");
        return false;
    }

    if (conf->set("bootstrap.servers", brokers_, errstr) != RdKafka::Conf::CONF_OK ||
        conf->set("queue.buffering.max.ms", "10", errstr) != RdKafka::Conf::CONF_OK ||
        conf->set("message.send.max.retries", "3", errstr) != RdKafka::Conf::CONF_OK ||
        conf->set("retry.backoff.ms", "100", errstr) != RdKafka::Conf::CONF_OK) {
        
        LOG_ERROR("Configuration error: {}", errstr);
        delete conf;
        return false;
    }

    RdKafka::Producer* producer_raw = RdKafka::Producer::create(conf, errstr);
    if (!producer_raw) {
        LOG_ERROR("Failed to create producer: {}", errstr);
        delete conf;
        return false;
    }

    producer_.reset(producer_raw);

    LOG_INFO("Started successfully, brokers={}, topic={}", brokers_, topic_);
    return true;
}

void KafkaProducer::stop() {
    if (!producer_) return;

    RdKafka::ErrorCode err = producer_->flush(5000);
    
    if (err == RdKafka::ERR_NO_ERROR) {
        LOG_INFO("All outstanding messages flushed successfully.");
    } else if (err == RdKafka::ERR__TIMED_OUT) {
        LOG_WARN("Flush timed out! Some messages might be lost. Remaining queue count: {}", 
                     producer_->outq_len());
    } else {
        LOG_ERROR("Flush failed with error: {}", RdKafka::err2str(err));
    }

    producer_.reset();

    LOG_INFO("Producer stopped.");
}

bool KafkaProducer::push(const std::string& key, const std::string& payload) {
    if (!producer_) {
        LOG_ERROR("push() called but producer not started.");
        return false;
    }

    int retry_count = 0;
    RdKafka::ErrorCode err;

    while (retry_count < 5) {
        err = producer_->produce(
            topic_,
            RdKafka::Topic::PARTITION_UA,
            RdKafka::Producer::RK_MSG_COPY,
            const_cast<char*>(payload.data()), payload.size(),
            key.empty() ? nullptr : key.data(), key.size(),
            0,
            nullptr
        );

        if (err == RdKafka::ERR__QUEUE_FULL) {
            retry_count++;
            LOG_WARN("Kafka internal queue full, polling and retrying... ({}/5)", retry_count);
            producer_->poll(10);
            continue;
        }

        break;
    }

    if (err != RdKafka::ERR_NO_ERROR) {
        LOG_ERROR("Produce failed permanently: {}", RdKafka::err2str(err));
        return false;
    }

    producer_->poll(0);

    return true;
}