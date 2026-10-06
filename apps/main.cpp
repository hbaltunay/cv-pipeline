#include <atomic>
#include <chrono>
#include <csignal>
#include <thread>

#include "logger.hpp"
#include "meta.hpp"
#include "multi.hpp"
#include "detect.hpp"
#include "worker.hpp"
#include "broker.hpp"
#include "api.hpp"
#include "pipeline.hpp"
#include "queue.hpp"

static std::atomic<bool> g_run{true};
Queue<FrameMeta> cv_queue(50);


static void on_signal(int) {
    g_run = false;
    cv_queue.close();
}

int main(int argc, char *argv[]) {
    Logger::init();
    std::signal(SIGINT,  on_signal);
    std::signal(SIGTERM, on_signal);

    Queue<SourceMeta> src_queue(50);
    Queue<FrameMeta>  pre_queue(50);
    Queue<FrameMeta>  infer_queue(50);

    TensorSpec spec;
    spec.input_name = "images"; spec.output_name = "output0"; spec.height = 640; spec.width = 640;

    Pipeline pipeline("CV");

    pipeline.add(std::make_unique<MultiStream>(src_queue, pre_queue));
    pipeline.add(std::make_unique<InferDetect>("yolo_ensemble", "1", "127.0.0.1:8001", 
        TritonProtocol::GRPC, pre_queue, infer_queue, false, spec));
    pipeline.add(std::make_unique<CVWorker>(infer_queue, cv_queue));
    pipeline.add(std::make_unique<KafkaBroker>(cv_queue, "127.0.0.1:9094", "result-topic"));

    pipeline.add(std::make_unique<Server>(src_queue, "127.0.0.1:8080"));

    pipeline.start();

    FrameMeta meta;

    while (g_run && cv_queue.pop(meta)) {
        LOG_INFO("Detection Size: {}", meta.detections.size());
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    pipeline.stop();

    return 0;
}