#include <iostream>
#include <nlohmann/json.hpp>

#include "logger.hpp"
#include "broker.hpp"


KafkaBroker::KafkaBroker(Queue<FrameMeta>& input_queue, const std::string& brokers, const std::string& topic)
    : input_queue_(input_queue),
      circuit_breaker_(10, std::chrono::seconds(30)),
      producer_(brokers, topic) {}

KafkaBroker::~KafkaBroker() {
    stop();
}

bool KafkaBroker::start() {
    if (running_.exchange(true)) return false;

    if (!producer_.start()) {
        LOG_CRITICAL("Failed to start Kafka producer — worker will not start.");
        return false;
    }

    broker_thread_ = std::thread(&KafkaBroker::run, this);
    LOG_INFO("Kafka broker started.");
    return true;
}

void KafkaBroker::stop() {
    if (!running_.exchange(false)) return;

    input_queue_.close();

    if (broker_thread_.joinable()) broker_thread_.join();
    producer_.stop();
    LOG_INFO("The Kafka broker stopped.");
}

std::string KafkaBroker::frameMetaToJson(const FrameMeta& meta) {
    nlohmann::json j;
    j["camera_id"] = meta.camera_id;
    j["frame_num"] = meta.frame_num;
    j["in_frame"] = meta.in_frame;

    j["detections"] = nlohmann::json::array();
    for (const auto& d : meta.detections) {
        j["detections"].push_back({
            {"class_id", d.cls},
            {"confidence", d.conf},
            {"bbox", {
                {"x", d.x}, {"y", d.y}, {"w", d.w}, {"h", d.h}
            }}
        });
    }

    return j.dump();
}

void KafkaBroker::publish(const FrameMeta& meta) {
    std::string key = std::to_string(meta.camera_id);
    std::string payload = frameMetaToJson(meta);

    if (!producer_.push(key, payload)) {
        throw std::runtime_error("Kafka push failed");
    }
}

void KafkaBroker::run() {
    FrameMeta meta;
    while (input_queue_.pop(meta)) {

        if (!circuit_breaker_.allowRequest()) {
            LOG_DEBUG("Circuit open — dropping frame #{}.", meta.frame_num);
            continue;
        }

        try {
            publish(meta);
            circuit_breaker_.recordSuccess();

        } catch (const std::exception& e) {
            LOG_ERROR("Frame #{} publish failed (failures: {}): {}",
                          meta.frame_num, circuit_breaker_.getFailureCount() + 1, e.what());
            circuit_breaker_.recordFailure();

        } catch (...) {
            LOG_ERROR("Unknown exception on frame #{}.", meta.frame_num);
            circuit_breaker_.recordFailure();
        }
    }
    LOG_INFO("Kafka broker run() loop exited.");
}