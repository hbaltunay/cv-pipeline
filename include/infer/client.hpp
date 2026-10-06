#pragma once
 
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <opencv2/core.hpp>
 
 
enum class TritonProtocol {
    HTTP,
    GRPC
};
 
struct TensorSpec {
    std::string input_name  = "images";
    std::string output_name = "output0";
    int height = 640;
    int width  = 640;
};
 
struct InferOutput {
    std::vector<int64_t> shape;
    std::vector<float>   data;
};

class IClient {
public:
    virtual ~IClient() = default;
    virtual bool is_live() = 0;
    virtual bool is_model_ready(const std::string& name, const std::string& version) = 0;
    virtual bool infer(const std::string& model, const std::string& version,
                       const TensorSpec& spec, const std::vector<cv::Mat>& frames,
                       uint64_t timeout_us, InferOutput& out) = 0;
};
 
class TritonClient {
private:
    std::string url_;
    TritonProtocol protocol_;
    bool verbose_;
 
    std::unique_ptr<IClient> client_;
 
    TensorSpec spec_;
    uint64_t timeout_us_;
 
public:
    TritonClient(const std::string& url, TritonProtocol protocol, bool verbose = false,
                 TensorSpec spec = TensorSpec(), uint64_t timeout_us = 100000);
    ~TritonClient();
 
    bool start();
    void stop();
    bool is_model_ready(const std::string& model_name, const std::string& model_version = "1");
 
    bool infer(const std::string& model_name, const std::string& model_version,
               const std::vector<cv::Mat>& frames, InferOutput& out);
};