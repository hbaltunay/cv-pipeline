#pragma once

#include <string>
#include <gst/gst.h>
#include <functional>


class Stream {
private:
    int camera_id_;
    std::string name_;
    std::string source_uri_;

    GstElement *decodebin_;
    GstElement *parent_pipeline_;
    GstPad* mixer_sink_pad_;

    std::function<void(int)> on_eos_callback_;

    static void onPadAdded(GstElement *src, GstPad *pad, gpointer user_data);
    static GstPadProbeReturn stampCameraId(GstPad* pad, GstPadProbeInfo* info, gpointer user_data);
    static GstPadProbeReturn onEosEvent(GstPad* pad, GstPadProbeInfo* info, gpointer user_data);

public:

    Stream(int camera_id, const std::string &name, const std::string &source_uri, GstElement *pipeline);
    ~Stream();
    
    bool attach();
    void detach();

    void setEosCallback(std::function<void(int)> cb) { on_eos_callback_ = cb; }

    int getId() const { return camera_id_; }
    const std::string& getName() const { return name_; }
    const std::string& getUri() const { return source_uri_; }
};