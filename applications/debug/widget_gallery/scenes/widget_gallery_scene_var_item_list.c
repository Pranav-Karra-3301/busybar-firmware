#include "../widget_gallery_i.h"

typedef struct {
    uint8_t dummy;
} WidgetGallerySceneVarItemList;

static void widget_gallery_scene_var_item_list_on_enter(void* context) {
    furi_assert(context);
    WidgetGallery* instance = context;
    UNUSED(instance);
}

static void widget_gallery_scene_var_item_list_on_exit(void* context) {
    furi_assert(context);
    WidgetGallery* instance = context;

    UNUSED(instance);
}

static bool
    widget_gallery_scene_var_item_list_on_event(const SceneManagerEvent* event, void* context) {
    furi_assert(event);
    furi_assert(context);

    WidgetGallery* instance = context;
    UNUSED(instance);

    bool consumed = false;

    return consumed;
}

const Scene widget_gallery_scene_var_item_list = {
    .enter_callback = widget_gallery_scene_var_item_list_on_enter,
    .exit_callback = widget_gallery_scene_var_item_list_on_exit,
    .event_callback = widget_gallery_scene_var_item_list_on_event,
    .data_size = sizeof(WidgetGallerySceneVarItemList),
};
