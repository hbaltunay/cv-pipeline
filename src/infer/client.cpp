#include "http_client.h"
#include "grpc_client.h"
 
#include "client.hpp"
#include "logger.hpp"

 
namespace tc = triton::client;
 
template <typename ClientType>
class GenericClient : public IClient {
private:
    std::unique_ptr<ClientType> client_;
 
public:
    GenericClient(std::unique_ptr<ClientType> client) : client_(std::move(client)) {}
 
    bool is_live() override {
        bool live = false;
        return client_->IsServerLive(&live).IsOk() && live;
    }
 
    bool is_model_ready(const std::string& name, const std::string& version) override {
        bool ready = false;
        return client_->IsModelReady(&ready, name, version).IsOk() && ready;
    }
 
    bool infer(const std::string& model, const std::string& version,
               const TensorSpec& spec, const std::vector<cv::Mat>& frames,
               uint64_t timeout_us, InferOutput& out) override {
        const int64_t n = static_cast<int64_t>(frames.size());
        const size_t row_bytes = static_cast<size_t>(spec.width) * 3;
        const size_t frame_bytes = row_bytes * static_cast<size_t>(spec.height);
 
        // 1) Input Tensor: UINT8, NHWC {N, H, W, 3}
        std::vector<int64_t> shape{n, spec.height, spec.width, 3};
        tc::InferInput* in_raw = nullptr;
        tc::Error err = tc::InferInput::Create(&in_raw, spec.input_name, shape, "UINT8");
        if (!err.IsOk()) {
            LOG_ERROR("InferInput create failed: {}", err.Message());
            return false;
        }
        std::unique_ptr<tc::InferInput> input(in_raw);
 
        for (const cv::Mat& f : frames) {
            if (f.isContinuous()) {
                err = input->AppendRaw(f.data, frame_bytes);
            } else {
                for (int r = 0; r < f.rows && err.IsOk(); ++r) {
                    err = input->AppendRaw(f.ptr(r), row_bytes);
                }
            }
            if (!err.IsOk()) {
                LOG_ERROR("AppendRaw failed: {}", err.Message());
                return false;
            }
        }
 
        tc::InferRequestedOutput* out_raw = nullptr;
        err = tc::InferRequestedOutput::Create(&out_raw, spec.output_name);
        if (!err.IsOk()) {
            LOG_ERROR("InferRequestedOutput create failed: {}", err.Message());
            return false;
        }
        std::unique_ptr<tc::InferRequestedOutput> output(out_raw);
 
        tc::InferOptions options(model);
        options.model_version_  = version;
        options.client_timeout_ = timeout_us;
 
        tc::InferResult* res_raw = nullptr;
        err = client_->Infer(&res_raw, options, {input.get()}, {output.get()});
        std::unique_ptr<tc::InferResult> result(res_raw);
        if (!err.IsOk() || !result) {
            LOG_ERROR("Infer failed: {}", err.Message());
            return false;
        }
        err = result->RequestStatus();
        if (!err.IsOk()) {
            LOG_ERROR("Infer request status: {}", err.Message());
            return false;
        }
 
        std::vector<int64_t> out_shape;
        std::string dtype;
        const uint8_t* buf = nullptr;
        size_t size = 0;
        if (!result->Shape(spec.output_name, &out_shape).IsOk() ||
            !result->Datatype(spec.output_name, &dtype).IsOk() ||
            !result->RawData(spec.output_name, &buf, &size).IsOk()) {
            LOG_ERROR("Could not read output '{}'.", spec.output_name);
            return false;
        }
        if (dtype != "FP32") {
            LOG_ERROR("Unexpected output dtype: {} (expected FP32)", dtype);
            return false;
        }
 
        const float* p = reinterpret_cast<const float*>(buf);
        out.shape = std::move(out_shape);
        out.data.assign(p, p + size / sizeof(float));
        return true;
    }
};
 
 
TritonClient::TritonClient(const std::string& url, TritonProtocol protocol, bool verbose,
                           TensorSpec spec, uint64_t timeout_us)
    : url_(url), protocol_(protocol), verbose_(verbose), client_(nullptr),
      spec_(std::move(spec)), timeout_us_(timeout_us) {}
 
 
TritonClient::~TritonClient() {
    stop();
}
 
bool TritonClient::start() {
    tc::Error err;
 
    if (protocol_ == TritonProtocol::HTTP) {
        std::unique_ptr<tc::InferenceServerHttpClient> http_ptr;
        err = tc::InferenceServerHttpClient::Create(&http_ptr, url_, verbose_);
        if (err.IsOk()) {
            client_ = std::make_unique<GenericClient<tc::InferenceServerHttpClient>>(std::move(http_ptr));
        }
    }
    else if (protocol_ == TritonProtocol::GRPC) {
        std::unique_ptr<tc::InferenceServerGrpcClient> grpc_ptr;
        err = tc::InferenceServerGrpcClient::Create(&grpc_ptr, url_, verbose_);
        if (err.IsOk()) {
            client_ = std::make_unique<GenericClient<tc::InferenceServerGrpcClient>>(std::move(grpc_ptr));
        }
    }
 
    if (!err.IsOk() || !client_) {
        LOG_ERROR("Client creation failed: {}", err.Message());
        return false;
    }
 
    if (!client_->is_live()) {
        LOG_ERROR("Server at {} is not live.", url_);
        stop();
        return false;
    }
 
    LOG_INFO("Triton client connected successfully.");
    return true;
}
 
bool TritonClient::is_model_ready(const std::string& model_name, const std::string& model_version) {
    if (!client_) {
        LOG_ERROR("Client is not started.");
        return false;
    }
    return client_->is_model_ready(model_name, model_version);
}
 
bool TritonClient::infer(const std::string& model_name, const std::string& model_version,
                         const std::vector<cv::Mat>& frames, InferOutput& out) {
    if (!client_) {
        LOG_ERROR("Client is not started.");
        return false;
    }
    if (frames.empty()) {
        return false;
    }
    return client_->infer(model_name, model_version, spec_, frames, timeout_us_, out);
}
 
void TritonClient::stop() {
    if (!client_) return;
    client_.reset();
    LOG_INFO("Triton client stopped.");
}