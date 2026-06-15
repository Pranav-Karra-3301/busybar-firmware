#include "../widget_gallery_i.h"

typedef struct {
    uint8_t dummy;
} WidgetGallerySceneLabel;

static void widget_gallery_scene_label_on_enter(void* context) {
    furi_assert(context);
    WidgetGallery* instance = context;
    UNUSED(instance);
}

static void widget_gallery_scene_label_on_exit(void* context) {
    furi_assert(context);
    WidgetGallery* instance = context;

    UNUSED(instance);
}

static bool widget_gallery_scene_label_on_event(const SceneManagerEvent* event, void* context) {
    furi_assert(event);
    furi_assert(context);

    WidgetGallery* instance = context;
    UNUSED(instance);

    bool consumed = false;

    return consumed;
}

const Scene widget_gallery_scene_label = {
    .enter_callback = widget_gallery_scene_label_on_enter,
    .exit_callback = widget_gallery_scene_label_on_exit,
    .event_callback = widget_gallery_scene_label_on_event,
    .data_size = sizeof(WidgetGallerySceneLabel),
};
