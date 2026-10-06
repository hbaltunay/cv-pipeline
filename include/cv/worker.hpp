#pragma once

#include <thread>
#include <atomic>
#include <opencv2/opencv.hpp>

#include "base.hpp"
#include "queue.hpp"
#include "meta.hpp"


class CVWorker : public Process {
private:
    Queue<FrameMeta>& input_queue_;
    Queue<FrameMeta>& output_queue_;

    std::thread worker_thread_;
    std::atomic<bool> running_{false};

    void run();
    void processFrame(FrameMeta& meta);

public:
    CVWorker(Queue<FrameMeta>& input_queue, Queue<FrameMeta>& output_queue);
    ~CVWorker();

    bool start() override;
    void stop() override;
};