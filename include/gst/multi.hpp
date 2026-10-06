#pragma once

#include <map>
#include <vector>
#include <atomic>
#include <thread>
#include <gst/gst.h>

#include "base.hpp"
#include "queue.hpp"
#include "meta.hpp"
#include "stream.hpp"


struct StreamInfo {
    int camera_id;
    std::string name;
    std::string uri;
};

class MultiStream : public Process {
private:
    Queue<SourceMeta>& src_queue_;
    Queue<FrameMeta>& output_queue_;

    guint bus_watch_id_;

    std::map<int, std::unique_ptr<Stream>> active_streams_;
    std::mutex stream_mutex_;

    std::atomic<bool> running_{false};
    std::atomic<int> next_camera_id_{1};

    // Gstreamer Plugins
    GstElement *pipeline_;
    GstElement *mixer_;
    GstElement *convert_;
    GstElement *scaler_;
    GstElement *capsfilter_;
    GstElement *appsink_;
    GMainLoop  *loop_;
    std::thread loop_thread_;
    std::thread src_thread_;

    void run();
    void srcRun();
    static GstFlowReturn onNewSample(GstElement *sink, gpointer user_data);
    static gboolean onBusMessage(GstBus *bus, GstMessage *msg, gpointer user_data);

public:
    MultiStream(Queue<SourceMeta>& src_queue, Queue<FrameMeta>& output_queue);
    ~MultiStream();

    bool start() override;
    void stop() override;
    void init();

    int addStream(const std::string& name, const std::string& uri);
    void removeStream(int camera_id);
    std::vector<StreamInfo> getActiveStreams();

    GstElement* getMixerBin() { return mixer_; }
    Queue<FrameMeta>& getPreQueue() { return output_queue_; }
};