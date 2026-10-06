#pragma once
 
#include <atomic>
#include <string>
#include <thread>
#include <vector>
 
#include "base.hpp"
#include "queue.hpp"
#include "meta.hpp"
#include "client.hpp"
#include "circuit_breaker.hpp"
 
 
class InferDetect : public Process {
private:
    static constexpr size_t kMaxBatch = 4;

    std::string model_;
    std::string version_;
    std::string url_;
    TritonProtocol protocol_;
 
    Queue<FrameMeta>& input_queue_;
    Queue<FrameMeta>& output_queue_;
 
    bool verbose_;
 
    std::thread worker_thread_;
    std::atomic<bool> running_{false};
 
    CircuitBreaker circuit_breaker_;
 
    TensorSpec spec_;
    TritonClient client_;

    bool mock_;
    void runMock();

    void run();
    bool validFrame(const FrameMeta& m) const;
    void postprocess(const InferOutput& out, std::vector<FrameMeta>& batch);
 
public:
    InferDetect(std::string model, std::string version, std::string url, TritonProtocol protocol,
        Queue<FrameMeta>& input_queue, Queue<FrameMeta>& output_queue,
        bool verbose = false, TensorSpec spec = TensorSpec(), bool mock = false);
    ~InferDetect();
 
    bool start() override;
    void stop() override;
};