#include "../widget_gallery_i.h"

#include <gui/modules/submenu.h>

typedef enum {
    WidgetGallerySceneStartIdxWidget,
    WidgetGallerySceneStartIdxLabel,
    WidgetGallerySceneStartIdxImage,
    WidgetGallerySceneStartIdxCanvas,
    WidgetGallerySceneStartIdxMenu,
    WidgetGallerySceneStartIdxSubmenu,
    WidgetGallerySceneStartIdxVarItemList,
    WidgetGallerySceneStartIdxAnimPlayer,
    WidgetGallerySceneStartIdxMax,
} WidgetGallerySceneStartIdx;

typedef struct {
    const char* const text;
    WidgetGallerySceneId scene_id;
} WidgetGallerySceneStartItem;

typedef struct {
    Submenu* submenus[GuiDisplayIdMax];
    WidgetGallerySceneStartIdx item_idx;
} WidgetGallerySceneStart;

static const WidgetGallerySceneStartItem widget_gallery_scene_start_items[] = {
    [WidgetGallerySceneStartIdxWidget] = {"Widget", WidgetGallerySceneIdWidget},
    [WidgetGallerySceneStartIdxLabel] = {"Label", WidgetGallerySceneIdLabel},
    [WidgetGallerySceneStartIdxImage] = {"Image", WidgetGallerySceneIdImage},
    [WidgetGallerySceneStartIdxCanvas] = {"Canvas", WidgetGallerySceneIdCanvas},
    [WidgetGallerySceneStartIdxMenu] = {"Menu", WidgetGallerySceneIdMenu},
    [WidgetGallerySceneStartIdxSubmenu] = {"Submenu", WidgetGallerySceneIdSubmenu},
    [WidgetGallerySceneStartIdxVarItemList] = {"VarItemList", WidgetGallerySceneIdVarItemList},
    [WidgetGallerySceneStartIdxAnimPlayer] = {"AnimPlayer", WidgetGallerySceneIdAnimPlayer},
};

static_assert(COUNT_OF(widget_gallery_scene_start_items) == WidgetGallerySceneStartIdxMax);

static void widget_gallery_scene_start_submenu_callback(uint32_t index, void* context) {
    furi_assert(context);
    WidgetGallery* instance = context;

    widget_gallery_send_custom_event(instance, index);
}

static void widget_gallery_scene_start_fill_submenu(
    WidgetGallery* instance,
    Submenu* submenu,
    bool add_callbacks) {
    SubmenuItemCallback item_callback =
        add_callbacks ? widget_gallery_scene_start_submenu_callback : NULL;

    for(uint32_t i = 0; i < WidgetGallerySceneStartIdxMax; ++i) {
        const char* item_text = widget_gallery_scene_start_items[i].text;
        submenu_add_item(submenu, item_text, NULL, i, item_callback, instance);
    }
}

static void widget_gallery_scene_start_on_enter(void* context) {
    furi_assert(context);
    WidgetGallery* instance = context;

    WidgetGallerySceneStart* data =
        scene_manager_get_scene_data(instance->scene_manager, WidgetGallerySceneIdStart);

    with_gui(instance->gui, {
        for(GuiDisplayId id = 0; id < GuiDisplayIdMax; ++id) {
            Widget* window = instance->windows[id];

            Submenu* submenu = submenu_alloc(window);

            widget_gallery_scene_start_fill_submenu(instance, submenu, (id == GuiDisplayIdFront));
            widget_set_scrollbar_enabled(submenu_get_base(submenu), true);
            submenu_set_selected_item_index(submenu, data->item_idx);

            data->submenus[id] = submenu;
        }
    });
}

static void widget_gallery_scene_start_on_exit(void* context) {
    furi_assert(context);
    WidgetGallery* instance = context;

    WidgetGallerySceneStart* data =
        scene_manager_get_scene_data(instance->scene_manager, WidgetGallerySceneIdStart);

    with_gui(instance->gui, {
        for(GuiDisplayId id = 0; id < GuiDisplayIdMax; ++id) {
            submenu_free(data->submenus[id]);
        }
    });
}

static bool widget_gallery_scene_start_on_event(const SceneManagerEvent* event, void* context) {
    furi_assert(event);
    furi_assert(context);

    WidgetGallery* instance = context;

    bool consumed = false;

    if(event->type == SceneManagerEventTypeCustom) {
        furi_assert(event->event < WidgetGallerySceneStartIdxMax);
        const WidgetGallerySceneStartIdx item_idx = event->event;

        WidgetGallerySceneStart* data =
            scene_manager_get_scene_data(instance->scene_manager, WidgetGallerySceneIdStart);
        data->item_idx = item_idx;

        const WidgetGallerySceneId scene_id = widget_gallery_scene_start_items[item_idx].scene_id;
        scene_manager_next_scene(instance->scene_manager, scene_id);
    }

    return consumed;
}

const Scene widget_gallery_scene_start = {
    .enter_callback = widget_gallery_scene_start_on_enter,
    .exit_callback = widget_gallery_scene_start_on_exit,
    .event_callback = widget_gallery_scene_start_on_event,
    .data_size = sizeof(WidgetGallerySceneStart),
};
