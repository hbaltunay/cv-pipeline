#include "meta.hpp"


GType camera_id_meta_api_get_type(void) {
    static GType type = 0;
    static const gchar* tags[] = { NULL };
    if (g_once_init_enter(&type)) {
        GType _type = gst_meta_api_type_register("CameraIdMetaAPI", tags);
        g_once_init_leave(&type, _type);
    }
    return type;
}

static gboolean camera_id_meta_init(GstMeta* meta, gpointer, GstBuffer*) {
    CameraIdMeta* cmeta = (CameraIdMeta*)meta;
    cmeta->camera_id = -1;
    return TRUE;
}

static gboolean camera_id_meta_transform(GstBuffer* dest, GstMeta* meta,
                                         GstBuffer* buffer, GQuark type, 
                                         gpointer data) {
    if (type == g_quark_from_static_string("gst-copy")) {
        CameraIdMeta* src_meta = (CameraIdMeta*)meta;
        gst_buffer_add_camera_id_meta(dest, src_meta->camera_id);
    }
    return TRUE;
}

const GstMetaInfo* camera_id_meta_get_info(void) {
    static const GstMetaInfo* info = NULL;
    if (g_once_init_enter(&info)) {
        const GstMetaInfo* mi = gst_meta_register(
            CAMERA_ID_META_API_TYPE,
            "CameraIdMeta",
            sizeof(CameraIdMeta),
            camera_id_meta_init,
            (GstMetaFreeFunction)NULL,
            camera_id_meta_transform
        );
        g_once_init_leave(&info, mi);
    }
    return info;
}

CameraIdMeta* gst_buffer_add_camera_id_meta(GstBuffer* buffer, gint camera_id) {
    CameraIdMeta* meta = (CameraIdMeta*)gst_buffer_get_meta(buffer, camera_id_meta_api_get_type());
    
    if (meta == NULL) {
        meta = (CameraIdMeta*)gst_buffer_add_meta(buffer, CAMERA_ID_META_INFO, NULL);
    } 

    meta->camera_id = camera_id;
    return meta;
}