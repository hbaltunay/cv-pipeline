#pragma once

#include <iostream>
#include <gst/gst.h>
#include <opencv2/core.hpp>
#include <cstddef>


class GstBufferMatAllocator : public cv::MatAllocator {
public:
    static GstBufferMatAllocator* instance();

    static cv::Mat wrap(GstSample* sample, int width, int height,
                         int cv_type = CV_8UC3, std::size_t stride = 0);

    cv::UMatData* allocate(int dims, const int* sizes, int type,
                            void* data, size_t* step,
                            cv::AccessFlag flags,
                            cv::UMatUsageFlags usageFlags) const override;

    bool allocate(cv::UMatData* u, cv::AccessFlag accessFlags,
                  cv::UMatUsageFlags usageFlags) const override;

    void deallocate(cv::UMatData* u) const override;

private:
    struct Context
    {
        GstSample* sample = nullptr;
        GstBuffer* buffer = nullptr;
        GstMapInfo map{};

        ~Context();
    };

    GstBufferMatAllocator() = default;
    ~GstBufferMatAllocator() override = default;

    GstBufferMatAllocator(const GstBufferMatAllocator&) = delete;
    GstBufferMatAllocator& operator=(const GstBufferMatAllocator&) = delete;
};