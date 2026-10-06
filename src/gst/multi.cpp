#include <iostream>
#include <gst/video/video.h>

#include "logger.hpp"
#include "multi.hpp"
#include "bridge.hpp"
#include "meta.hpp"


MultiStream::MultiStream(Queue<SourceMeta>& src_queue, Queue<FrameMeta>& output_queue)
    : src_queue_(src_queue),
      output_queue_(output_queue), 
      pipeline_(nullptr),
      mixer_(nullptr),
      convert_(nullptr),
      capsfilter_(nullptr),
      appsink_(nullptr), 
      loop_(nullptr) {
    gst_init(nullptr, nullptr);
    init();
}

MultiStream::~MultiStream() {
    stop(); 
}

void MultiStream::init() {

    loop_     = g_main_loop_new(nullptr, FALSE);
    pipeline_ = gst_pipeline_new("multi-uri-core-pipeline");

    mixer_      = gst_element_factory_make("funnel", "mixer");
    convert_    = gst_element_factory_make("videoconvert", "converter");
    scaler_     = gst_element_factory_make("videoscale", "scaler");
    capsfilter_ = gst_element_factory_make("capsfilter", "filter");
    appsink_    = gst_element_factory_make("appsink", "sink");

    if (!pipeline_ || !mixer_ || !convert_ || !scaler_ || !capsfilter_ || !appsink_) {
        LOG_ERROR("Basic elements could not be created.!");
        return;
    }

    g_object_set(G_OBJECT(appsink_),
        "emit-signals", TRUE, 
        "max-buffers", 1, NULL);

    GstCaps* caps = gst_caps_from_string("video/x-raw, format=BGR, width=640, height=640");
    g_object_set(G_OBJECT(capsfilter_), "caps", caps, NULL);
    gst_caps_unref(caps);

    gst_bin_add_many(GST_BIN(pipeline_), mixer_, convert_, scaler_, capsfilter_, appsink_, NULL);

    if (!gst_element_link_many(mixer_, convert_, scaler_, capsfilter_, appsink_, NULL)) {
        LOG_ERROR("Main basic elements could not be linked!");
    }

    GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline_));
    bus_watch_id_ = gst_bus_add_watch(bus, MultiStream::onBusMessage, this);
    gst_object_unref(bus);

    g_signal_connect(appsink_, "new-sample", G_CALLBACK(MultiStream::onNewSample), this);

    LOG_INFO("Multi stream has been initialized.");
}

int MultiStream::addStream(const std::string& name, const std::string& uri) {
    std::lock_guard<std::mutex> lock(stream_mutex_);

    int new_id = next_camera_id_.fetch_add(1);

    LOG_INFO("The new camera is added independently of the line. Id: {} Name: {}", new_id, name);

    auto stream = std::make_unique<Stream>(new_id, name, uri, pipeline_);

    stream->setEosCallback([this](int camera_id) {
        LOG_INFO("Auto-removing camera {} after EOS.", camera_id);
        this->removeStream(camera_id); 
    });

    if (stream->attach()) {
        active_streams_[new_id] = std::move(stream);
        return new_id;
    }
    return -1;
}

void MultiStream::removeStream(int camera_id) {
    std::unique_ptr<Stream> stream_to_delete = nullptr;
    {
        std::lock_guard<std::mutex> lock(stream_mutex_);
        auto it = active_streams_.find(camera_id);
        if (it != active_streams_.end()) {
            stream_to_delete = std::move(it->second);
            active_streams_.erase(it);
        }
    }

    if (stream_to_delete) {
        stream_to_delete->detach(); 
        LOG_INFO("The stream has been removed. Camera Id: {}", camera_id);
    } else {
        LOG_INFO("The stream not found. Camera Id: {}", camera_id);
    }
}

std::vector<StreamInfo> MultiStream::getActiveStreams() {
    std::lock_guard<std::mutex> lock(stream_mutex_);
    std::vector<StreamInfo> result;
    result.reserve(active_streams_.size());
    for (const auto& [id, stream] : active_streams_) {
        result.push_back({id, stream->getName(), stream->getUri()});
    }
    return result;
}

bool MultiStream::start() {
    if (running_.exchange(true)) return false;
    if (loop_thread_.joinable()) {
        LOG_WARN("Multi stream is already running!");
        return false;
    }
    if (!pipeline_) init();

    loop_thread_ = std::thread(&MultiStream::run, this);
    gst_element_set_state(pipeline_, GST_STATE_PLAYING);

    running_ = true;
    src_thread_ = std::thread(&MultiStream::srcRun, this);

    LOG_INFO("Multi stream started.");

    return true;
}

void MultiStream::run() {
    g_main_loop_run(loop_);
}

void MultiStream::srcRun() {
    SourceMeta meta;
    while (src_queue_.pop(meta)) {
        switch (meta.task) {
            case Task::ADD:
                addStream(meta.name, meta.uri);
                break;
            case Task::REMOVE:
                removeStream(meta.camera_id);
                break;
            default:
                LOG_WARN("Not found task.");
                break;
        }
    }
}

void MultiStream::stop() {
    if (!running_.exchange(false)) return;
    if (loop_ && g_main_loop_is_running(loop_)) g_main_loop_quit(loop_);
    if (loop_thread_.joinable()) loop_thread_.join();

    src_queue_.close(); 
    
    if (src_thread_.joinable()) src_thread_.join();

    std::vector<int> stream_ids;
    {
        std::lock_guard<std::mutex> lock(stream_mutex_);
        for (const auto& [id, stream] : active_streams_) {
            stream_ids.push_back(id);
        }
    }
    for (int id : stream_ids) removeStream(id);

    if (bus_watch_id_ > 0) {
        g_source_remove(bus_watch_id_);
        bus_watch_id_ = 0;
    }
    if (pipeline_) {
        gst_element_set_state(pipeline_, GST_STATE_NULL);
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
    }
    if (loop_) {
        g_main_loop_unref(loop_);
        loop_ = nullptr;
    }

    LOG_INFO("Multi stream stopped.");
}

gboolean MultiStream::onBusMessage(GstBus* bus, GstMessage* msg, gpointer user_data) {
    MultiStream* self = static_cast<MultiStream*>(user_data);

    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ERROR: {
            GError* err = nullptr;
            gchar* dbg = nullptr;
            gst_message_parse_error(msg, &err, &dbg);

            const gchar* src_name = GST_MESSAGE_SRC_NAME(msg);
            std::string element_name = src_name ? src_name : "Unknown Element";

            LOG_ERROR("GStreamer Error from [{}]: {}", element_name, err->message);

            if (self && g_str_has_prefix(element_name.c_str(), "decodebin_")) {
                try {
                    int cam_id = std::stoi(element_name.substr(10));
                    LOG_WARN("Bus: Camera {} encountered a fatal error. Initiating auto-removal...", cam_id);
                    self->removeStream(cam_id);
                } catch (const std::exception& e) {
                    LOG_ERROR("Bus: Failed to parse camera ID from element name: {}", e.what());
                }
            }
            g_error_free(err);
            g_free(dbg);
            break;
        }
        case GST_MESSAGE_WARNING: {
            GError* err = nullptr;
            gchar* dbg = nullptr;
            gst_message_parse_warning(msg, &err, &dbg);

            const gchar* src_name = GST_MESSAGE_SRC_NAME(msg);
            std::string element_name = src_name ? src_name : "Unknown Element";

            LOG_WARN("GStreamer Warning from [{}]: {}", element_name, err->message);

            g_error_free(err);
            g_free(dbg);
            break;
        }
        default:
            break;
    }
    return TRUE;
}

GstFlowReturn MultiStream::onNewSample(GstElement* sink, gpointer user_data) {
    auto* receiver = static_cast<MultiStream*>(user_data);
    if (!receiver) return GST_FLOW_OK;

    GstSample* sample = nullptr;
    g_signal_emit_by_name(sink, "pull-sample", &sample);
    if (!sample) return GST_FLOW_OK;

    GstCaps* caps = gst_sample_get_caps(sample);
    if (!caps) {
        gst_sample_unref(sample);
        return GST_FLOW_OK;
    }

    GstStructure* structure = gst_caps_get_structure(caps, 0);
    int width = 0, height = 0;
    gst_structure_get_int(structure, "width", &width);
    gst_structure_get_int(structure, "height", &height);

    size_t stride = 0;
    GstBuffer* rawBuffer = gst_sample_get_buffer(sample);
    if (GstVideoMeta* vmeta = gst_buffer_get_video_meta(rawBuffer)) {
        stride = vmeta->stride[0];
    }

    FrameMeta meta;

    CameraIdMeta* cam_meta = gst_buffer_get_camera_id_meta(rawBuffer);
    meta.camera_id = cam_meta ? cam_meta->camera_id : -1;

    static std::atomic<uint64_t> global_counter{0};
    meta.frame_num = global_counter.fetch_add(1) + 1;
    meta.timestamp = std::chrono::steady_clock::now();

    meta.frame = GstBufferMatAllocator::wrap(sample, width, height, CV_8UC3, stride);

    if (meta.frame.empty()) {
        gst_sample_unref(sample);
        return GST_FLOW_OK;
    }

    receiver->output_queue_.push(std::move(meta));

    gst_sample_unref(sample);

    return GST_FLOW_OK;
}