#include <stdexcept>
#include <fstream>
#include <nlohmann/json.hpp>

#include "logger.hpp"
#include "detect.hpp"


using json = nlohmann::json;
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ObjectDetection, x, y, w, h, conf, cls)
 
InferDetect::InferDetect(std::string model, std::string version, std::string url, TritonProtocol protocol,
        Queue<FrameMeta>& input_queue, Queue<FrameMeta>& output_queue, bool verbose, TensorSpec spec, bool mock)
    : model_(model), version_(version), url_(url), protocol_(protocol),
      input_queue_(input_queue), output_queue_(output_queue), verbose_(verbose),
      circuit_breaker_(5, std::chrono::seconds(10)), spec_(spec), mock_(mock),
      client_(url, protocol, verbose, spec) {}
 
InferDetect::~InferDetect() {
    stop();   
}
 
bool InferDetect::start() {
    if (running_.exchange(true)) return false;

    if (mock_) {
        worker_thread_ = std::thread(&InferDetect::runMock, this);
        LOG_INFO("Infer detect started. [MOCK]");
        return true;
    }
 
    if (!client_.start()) {
        LOG_ERROR("Triton client could not be started.");
        running_ = false;
        return false;
    }
 
    if (!client_.is_model_ready(model_, version_)) {
        LOG_ERROR("Model not found.");
        client_.stop();
        running_ = false;
        return false;
    }
 
    worker_thread_ = std::thread(&InferDetect::run, this);
 
    LOG_INFO("Infer detect started.");
 
    return true;
}
 
void InferDetect::stop() {
    if (!running_.exchange(false)) return;
 
    input_queue_.close();
 
    if (worker_thread_.joinable()) worker_thread_.join();
    client_.stop();
    LOG_INFO("Infer detect stopped.");
}
 
bool InferDetect::validFrame(const FrameMeta& m) const {
    const cv::Mat& f = m.frame;
    return !f.empty()
        && f.type() == CV_8UC3
        && f.rows == spec_.height
        && f.cols == spec_.width
        && f.step[0] >= static_cast<size_t>(spec_.width) * 3;
}
 
void InferDetect::postprocess(const InferOutput& out, std::vector<FrameMeta>& batch) {
    const size_t batch_size = batch.size();

    if (out.shape.empty() || static_cast<size_t>(out.shape[0]) != batch_size || out.data.empty()) {
        throw std::runtime_error("Output batch size mismatch");
    }
    
    const size_t per_frame_elements = out.data.size() / batch_size; 
    const size_t box_elements = 6;
    const size_t num_boxes = per_frame_elements / box_elements; 

    for (size_t b = 0; b < batch_size; ++b) {
        const float* p = out.data.data() + b * per_frame_elements;

        for (size_t d = 0; d < num_boxes; ++d) {
            const float* box = p + (d * box_elements);

            if (box[4] <=0) continue;

            ObjectDetection det;

            det.x = (box[0] + box[2]) / 2;
            det.y = (box[1] + box[3]) / 2;
            det.w = box[2] - box[0];
            det.h = box[3] - box[1];
            det.conf = box[4];
            det.cls = static_cast<int>(box[5]);

            batch[b].detections.push_back(det);
        }
    }
}

void InferDetect::run() {
    std::vector<FrameMeta> batch;
    std::vector<cv::Mat>   frames;
    InferOutput            out;
 
    frames.reserve(kMaxBatch);

    while (input_queue_.pop_batch(batch, kMaxBatch, std::chrono::milliseconds(3000)) > 0) {
 
        if (!circuit_breaker_.allowRequest()) {
            LOG_DEBUG("Circuit open — dropping batch without attempting.");
            continue;
        }
 
        frames.clear();
        size_t n = 0;
        for (size_t i = 0; i < batch.size(); ++i) {
            if (!validFrame(batch[i])) {
                LOG_WARN("Invalid frame skipped (size/type/stride).");
                continue;
            }
            frames.push_back(batch[i].frame);
            if (n != i) batch[n] = std::move(batch[i]);
            ++n;
        }
        batch.erase(batch.begin() + n, batch.end());
        if (n == 0) continue;
 
        try {
            const bool ok = client_.infer(model_, version_, frames, out);
            frames.clear();
 
            if (!ok) {
                LOG_ERROR("Batch of {} frames dropped (failures: {})",
                          n, circuit_breaker_.getFailureCount() + 1);
                circuit_breaker_.recordFailure();
                continue;
            }
 
            postprocess(out, batch);
            for (auto& m : batch) {
                output_queue_.push(std::move(m));
            }
 
            circuit_breaker_.recordSuccess();
 
        } catch (const std::exception& e) {
            frames.clear();
            LOG_ERROR("Batch dropped (failures: {}): {}",
                      circuit_breaker_.getFailureCount() + 1, e.what());
            circuit_breaker_.recordFailure();
 
        } catch (...) {
            frames.clear();
            LOG_ERROR("Unknown exception on batch — dropped.");
            circuit_breaker_.recordFailure();
        }
    }
    LOG_INFO("Infer detect run() loop exited.");
}


void InferDetect::runMock() {
    std::vector<FrameMeta> batch;
    std::vector<cv::Mat>   frames;

    std::ifstream jsonl_file("assets/detections_output.jsonl");
    std::string json_line;

    while (input_queue_.pop_batch(batch, kMaxBatch, std::chrono::milliseconds(30)) > 0) {
 
        if (!circuit_breaker_.allowRequest()) {
            LOG_DEBUG("Circuit open — dropping batch without attempting.");
            continue;
        }
 
        frames.clear();
        size_t n = 0;
        for (size_t i = 0; i < batch.size(); ++i) {
            if (!validFrame(batch[i])) {
                LOG_WARN("Invalid frame skipped (size/type/stride).");
                continue;
            }
            frames.push_back(batch[i].frame);
            if (n != i) batch[n] = std::move(batch[i]);
            ++n;
        }
        batch.erase(batch.begin() + n, batch.end());
        if (n == 0) continue;
 
        try {
            frames.clear();
            
            for (auto& m : batch) {
                if (jsonl_file.is_open() && std::getline(jsonl_file, json_line)) {
                    if (!json_line.empty()) {
                        try {
                            json frame_json = json::parse(json_line);

                            uint64_t json_frame_num = frame_json["frame_num"];
                            
                            if (m.frame_num == json_frame_num) {
                                m.detections = frame_json["detections"].get<std::vector<ObjectDetection>>();
                            } else {
                                LOG_WARN("Synchronization Drift! Queue Frame: {}, JSON Frame: {}", m.frame_num, json_frame_num);
                            }

                        } catch (const json::parse_error& e) {
                            LOG_ERROR("JSON parse error: {}", e.what());
                        }
                    }
                } 
                output_queue_.push(std::move(m));
            }
 
            circuit_breaker_.recordSuccess();
 
        } catch (const std::exception& e) {
            frames.clear();
            LOG_ERROR("Batch dropped (failures: {}): {}",
                      circuit_breaker_.getFailureCount() + 1, e.what());
            circuit_breaker_.recordFailure();
 
        } catch (...) {
            frames.clear();
            LOG_ERROR("Unknown exception on batch — dropped.");
            circuit_breaker_.recordFailure();
        }
    }
    LOG_INFO("Infer detect run() loop exited.");
}