#include <iostream>

#include "logger.hpp"
#include "worker.hpp"


CVWorker::CVWorker(Queue<FrameMeta>& input_queue, Queue<FrameMeta>& output_queue)
    : input_queue_(input_queue), output_queue_(output_queue) {}

CVWorker::~CVWorker() {
    stop();
}

bool CVWorker::start() {
    if (running_.exchange(true)) return false;

    worker_thread_ = std::thread(&CVWorker::run, this);
    
    LOG_INFO("CV worker started.");

    return true;
}

void CVWorker::stop() {
    if (!running_.exchange(false)) return;

    input_queue_.close();

    if (worker_thread_.joinable()) worker_thread_.join();
    LOG_INFO("CV worker stopped.");
}

void CVWorker::processFrame(FrameMeta& meta) {}

void CVWorker::run() {
    FrameMeta meta;

    while (input_queue_.pop(meta)) {
        processFrame(meta);

        output_queue_.push(std::move(meta));

        LOG_TRACE("CV processing has been completed. [CamId: {} | FrameNum: {}]", 
            meta.camera_id, meta.frame_num);
    }

    LOG_INFO("CV worker run() loop exited.");
}