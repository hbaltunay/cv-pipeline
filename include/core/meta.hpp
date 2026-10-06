#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <gst/gst.h>
#include <opencv2/core.hpp>


struct ObjectDetection {
    float x, y, w, h;
    float conf;
    int cls;
};

struct FrameMeta {
    int camera_id;
    uint64_t frame_num;
    bool in_frame;
    
    cv::Mat frame;
    
    std::chrono::steady_clock::time_point timestamp;
    
    std::vector<ObjectDetection> detections;
};

enum class Task {
    ADD,
    REMOVE
};

struct SourceMeta {
    int source_id;
    int camera_id;
    std::string name;
    std::string uri;
    Task task;
};

typedef struct _CameraIdMeta {
    GstMeta meta;
    gint camera_id;
} CameraIdMeta;

GType camera_id_meta_api_get_type(void);
#define CAMERA_ID_META_API_TYPE (camera_id_meta_api_get_type())

const GstMetaInfo* camera_id_meta_get_info(void);
#define CAMERA_ID_META_INFO (camera_id_meta_get_info())

CameraIdMeta* gst_buffer_add_camera_id_meta(GstBuffer* buffer, gint camera_id);

#define gst_buffer_get_camera_id_meta(b) ((CameraIdMeta*)gst_buffer_get_meta((b), CAMERA_ID_META_API_TYPE))