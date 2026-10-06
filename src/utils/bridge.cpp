#include <limits>
#include <memory>

#include "bridge.hpp"


GstBufferMatAllocator::Context::~Context() {
    if (buffer) {
        gst_buffer_unmap(buffer, &map);
    }
    if (sample) {
        gst_sample_unref(sample);
    }
}

GstBufferMatAllocator* GstBufferMatAllocator::instance() {
    static GstBufferMatAllocator* inst = new GstBufferMatAllocator();
    return inst;
}

cv::UMatData* GstBufferMatAllocator::allocate(int, const int*, int, void*, size_t*,
                                              cv::AccessFlag, cv::UMatUsageFlags) const {
    CV_Error(cv::Error::StsNotImplemented,
             "GstBufferMatAllocator: use wrap() instead of allocate()");
}

bool GstBufferMatAllocator::allocate(cv::UMatData* u, cv::AccessFlag,
                                     cv::UMatUsageFlags) const {
    return u != nullptr;
}

void GstBufferMatAllocator::deallocate(cv::UMatData* u) const {
    if (!u) return;
    if (u->refcount > 0 || u->urefcount > 0) return;

    delete static_cast<Context*>(u->userdata);   
    delete u;
}

cv::Mat GstBufferMatAllocator::wrap(GstSample* sample, int width, int height,
                                     int cv_type, std::size_t stride) {
    if (!sample || width <= 0 || height <= 0) {
        return cv::Mat();
    }

    const std::size_t elemSize = static_cast<std::size_t>(CV_ELEM_SIZE(cv_type));
    if (elemSize == 0) {
        return cv::Mat();
    }

    GstBuffer* buffer = gst_sample_get_buffer(sample);
    if (!buffer) {
        return cv::Mat();
    }

    const std::size_t w = static_cast<std::size_t>(width);
    if (w > std::numeric_limits<std::size_t>::max() / elemSize) {
        return cv::Mat();
    }

    const std::size_t rowBytes  = w * elemSize;
    const std::size_t effStride = stride ? stride : rowBytes;

    if (effStride < rowBytes) {
        return cv::Mat();
    }

    const std::size_t rowsBefore = static_cast<std::size_t>(height) - 1;
    if (rowsBefore > (std::numeric_limits<std::size_t>::max() - rowBytes) / effStride) {
        return cv::Mat();
    }
    const std::size_t required = rowsBefore * effStride + rowBytes;

    auto ctx = std::make_unique<Context>();
    ctx->sample = gst_sample_ref(sample);

    if (!gst_buffer_map(buffer, &ctx->map, GST_MAP_READ)) {
        return cv::Mat();
    }

    ctx->buffer = buffer; 

    if (ctx->map.size < required) {
        return cv::Mat();
    }

    // UMatData
    auto* u = new cv::UMatData(instance());
    u->data = u->origdata = ctx->map.data;
    u->size = ctx->map.size;
    u->userdata = ctx.release();
    u->refcount = 0;
    u->flags |= cv::UMatData::USER_ALLOCATED;

    // Mat
    cv::Mat m;
    m.flags = cv::Mat::MAGIC_VAL | cv_type;
    if (effStride == rowBytes) {
        m.flags |= cv::Mat::CONTINUOUS_FLAG;
    }
    m.dims      = 2;
    m.rows      = height;
    m.cols      = width;
    m.step[0]   = effStride;
    m.step[1]   = elemSize;
    m.data      = u->data;
    m.datastart = u->data;
    m.dataend   = u->data + required;
    m.datalimit = m.dataend;
    m.allocator = instance();
    m.u         = u;
    m.addref();

    return m;
}