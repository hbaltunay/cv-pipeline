#include <iostream>

#include "logger.hpp"
#include "stream.hpp"
#include "meta.hpp"


Stream::Stream(int camera_id, const std::string& name, const std::string& source_uri, GstElement* parent_pipeline)
    : camera_id_(camera_id), 
      name_(name),
      source_uri_(source_uri), 
      parent_pipeline_(parent_pipeline), 
      decodebin_(nullptr),
      mixer_sink_pad_(nullptr) {}

Stream::~Stream() {
    detach();
}

bool Stream::attach() {
    std::string element_name = "decodebin_" + std::to_string(camera_id_);

    decodebin_ = gst_element_factory_make("uridecodebin", element_name.c_str());

    if (!decodebin_) {
        LOG_ERROR("Decodebin could not be created! Camera Id: {}", camera_id_);
        return false;
    }

    g_object_set(G_OBJECT(decodebin_), "uri", source_uri_.c_str(), NULL);

    gst_element_set_locked_state(decodebin_, TRUE);

    gst_bin_add(GST_BIN(parent_pipeline_), decodebin_);

    g_signal_connect(decodebin_, "pad-added", G_CALLBACK(Stream::onPadAdded), this);

    gst_element_set_locked_state(decodebin_, FALSE);

    if (!gst_element_sync_state_with_parent(decodebin_)) {
        LOG_ERROR("Decodebin state could not be synced with parent! Camera Id: {}", camera_id_);
        return false;
    }

    return true;
}

void Stream::detach() {
    if (!decodebin_) return;

    if (parent_pipeline_ && mixer_sink_pad_) {
        GstElement* mixer = gst_bin_get_by_name(GST_BIN(parent_pipeline_), "mixer");
        if (mixer) {
            gst_element_release_request_pad(mixer, mixer_sink_pad_);
            gst_object_unref(mixer);
        }
        mixer_sink_pad_ = nullptr;
    }

    gst_element_set_locked_state(decodebin_, TRUE);

    gst_element_set_state(decodebin_, GST_STATE_NULL);

    g_signal_handlers_disconnect_by_data(decodebin_, this);

    gst_bin_remove(GST_BIN(parent_pipeline_), decodebin_);

    decodebin_ = nullptr;

    LOG_INFO("Camera ({}) was disconnected from the line", name_);
}

GstPadProbeReturn Stream::stampCameraId(GstPad*, GstPadProbeInfo* info, gpointer user_data) {
    Stream* self = static_cast<Stream*>(user_data);

    if (!self || !info || !GST_PAD_PROBE_INFO_BUFFER(info)) {
        return GST_PAD_PROBE_OK;
    }

    GstBuffer* old_buffer = GST_PAD_PROBE_INFO_BUFFER(info);
    GstBuffer* writable_buffer = gst_buffer_make_writable(old_buffer);

    if (!writable_buffer) {
        LOG_ERROR("Camera ({}) buffer could not be made writable!", self->name_);
        return GST_PAD_PROBE_OK;
    }
    
    GST_PAD_PROBE_INFO_DATA(info) = writable_buffer;

    gst_buffer_add_camera_id_meta(writable_buffer, self->camera_id_);

    return GST_PAD_PROBE_OK;
}

GstPadProbeReturn Stream::onEosEvent(GstPad* pad, GstPadProbeInfo* info, gpointer user_data) {
    GstEvent* event = GST_PAD_PROBE_INFO_EVENT(info);

    if (event && GST_EVENT_TYPE(event) == GST_EVENT_EOS) {
        Stream* self = static_cast<Stream*>(user_data);
        LOG_INFO("Camera ({}) reached EOS. Processing disconnect...", self->name_);

        if (self->on_eos_callback_) {
            int cam_id = self->camera_id_;
            auto* callback_data = new std::pair<Stream*, int>(self, cam_id);

            g_idle_add([](gpointer data) -> gboolean {
                if (!data) return G_SOURCE_REMOVE;

                auto* pair = static_cast<std::pair<Stream*, int>*>(data);
                pair->first->on_eos_callback_(pair->second);
                delete pair;
                return G_SOURCE_REMOVE; 
            }, callback_data);
        }
        return GST_PAD_PROBE_DROP; 
    }
    return GST_PAD_PROBE_OK;
}

void Stream::onPadAdded(GstElement *src, GstPad *pad, gpointer user_data) {
    Stream *streamObj = static_cast<Stream*>(user_data);

    GstCaps *caps = gst_pad_query_caps(pad, nullptr);
    if (!caps || gst_caps_is_empty(caps)) {
        if (caps) gst_caps_unref(caps);
        return;
    }

    GstStructure* structure = gst_caps_get_structure(caps, 0);
    const gchar* name = gst_structure_get_name(structure);

    if (g_str_has_prefix(name, "video")) {
        GstElement* pipeline = GST_ELEMENT(gst_element_get_parent(src));
        if (!pipeline) {
            gst_caps_unref(caps);
            return;
        }

        GstClock* clock = gst_element_get_clock(pipeline);
        if (clock) {
            GstClockTime now = gst_clock_get_time(clock);
            GstClockTime base_time = gst_element_get_base_time(pipeline);
            GstClockTime running_time = now - base_time;

            gst_pad_set_offset(pad, running_time);

            LOG_INFO("Camera ({}) offset applied: {}s", streamObj->name_, GST_TIME_AS_SECONDS(running_time));

            gst_object_unref(clock);
        }

        gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER, Stream::stampCameraId, streamObj, nullptr);
        gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, Stream::onEosEvent, streamObj, nullptr);

        GstElement* mixer = gst_bin_get_by_name(GST_BIN(pipeline), "mixer");
        if (mixer) {
            GstPad* sink_pad = gst_element_request_pad_simple(mixer, "sink_%u");

            if (sink_pad) {
                if (gst_pad_link(pad, sink_pad) != GST_PAD_LINK_OK) {
                    LOG_ERROR("Camera ({}) could not connect to the mixer!", streamObj->name_);
                    gst_element_release_request_pad(mixer, sink_pad);
                } else {
                    LOG_INFO("Camera ({}) successfully connected to the central line.", streamObj->name_);
                    streamObj->mixer_sink_pad_ = sink_pad; 
                }
                gst_object_unref(sink_pad);
            }
            gst_object_unref(mixer);
        } else {
            LOG_ERROR("Mixer not found for camera ({})!", streamObj->name_);
        }
        gst_object_unref(pipeline);
    }
    gst_caps_unref(caps);
}
