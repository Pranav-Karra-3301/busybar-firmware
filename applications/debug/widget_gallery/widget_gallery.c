#include "widget_gallery_i.h"

#include "scenes/widget_gallery_scenes.h"

#define INPUT_QUEUE_SIZE (8)
#define EVENT_QUEUE_SIZE (8)

static void widget_gallery_input_queue_callback(FuriEventLoopObject* object, void* context) {
    furi_assert(context);

    WidgetGallery* instance = context;
    furi_assert(instance->input_queue == object);

    InputEvent event;
    while(furi_message_queue_get(instance->input_queue, &event, 0) == FuriStatusOk) {
        if(event.type == InputTypeShort) {
            if(event.key == InputKeyBack) {
                if(!scene_manager_handle_back_event(instance->scene_manager)) {
                    furi_event_loop_stop(instance->event_loop);
                }
            }
        }
    }
}

static void widget_gallery_event_queue_callback(FuriEventLoopObject* object, void* context) {
    furi_assert(context);

    WidgetGallery* instance = context;
    furi_assert(instance->event_queue == object);

    uint32_t event;
    while(furi_message_queue_get(instance->event_queue, &event, 0) == FuriStatusOk) {
        scene_manager_handle_custom_event(instance->scene_manager, event);
    }
}

static bool widget_gallery_gui_input_callback(const InputEvent* event, void* context) {
    furi_assert(event);
    furi_assert(context);

    bool consumed = false;

    WidgetGallery* instance = context;

    if(event->type == InputTypeShort) {
        if(event->key == InputKeyBack) {
            consumed = true;
        }
    }

    if(consumed) {
        furi_check(
            furi_message_queue_put(instance->input_queue, event, FuriWaitForever) == FuriStatusOk);
    }

    return consumed;
}

void widget_gallery_send_custom_event(WidgetGallery* instance, uint32_t event) {
    furi_check(
        furi_message_queue_put(instance->event_queue, &event, FuriWaitForever) == FuriStatusOk);
}

static WidgetGallery* widget_gallery_alloc(void) {
    WidgetGallery* instance = malloc(sizeof(WidgetGallery));

    instance->event_loop = furi_event_loop_alloc();
    instance->input_queue = furi_message_queue_alloc(INPUT_QUEUE_SIZE, sizeof(InputEvent));
    instance->event_queue = furi_message_queue_alloc(EVENT_QUEUE_SIZE, sizeof(uint32_t));
    instance->scene_manager =
        scene_manager_alloc(widget_gallery_scenes, WidgetGallerySceneIdMax, instance);
    instance->gui = furi_record_open(RECORD_GUI);

    furi_event_loop_subscribe_message_queue(
        instance->event_loop,
        instance->input_queue,
        FuriEventLoopEventIn,
        widget_gallery_input_queue_callback,
        instance);

    furi_event_loop_subscribe_message_queue(
        instance->event_loop,
        instance->event_queue,
        FuriEventLoopEventIn,
        widget_gallery_event_queue_callback,
        instance);

    with_gui(instance->gui, {
        GuiLayer* layer = gui_get_layer(instance->gui, GuiLayerIdMain);
        gui_layer_add_input_callback(layer, widget_gallery_gui_input_callback, instance);

        for(GuiDisplayId id = 0; id < GuiDisplayIdMax; ++id) {
            Widget* root = gui_layer_get_root_widget(layer, id);
            instance->windows[id] = widget_alloc(root);
        }
    });

    scene_manager_next_scene(instance->scene_manager, WidgetGallerySceneIdStart);

    return instance;
}

static void widget_gallery_free(WidgetGallery* instance) {
    with_gui(instance->gui, {
        GuiLayer* layer = gui_get_layer(instance->gui, GuiLayerIdMain);
        gui_layer_remove_input_callback(layer, widget_gallery_gui_input_callback);

        for(GuiDisplayId id = 0; id < GuiDisplayIdMax; ++id) {
            widget_free(instance->windows[id]);
        }
    });

    furi_record_close(RECORD_GUI);

    furi_event_loop_unsubscribe(instance->event_loop, instance->input_queue);
    furi_message_queue_free(instance->input_queue);

    furi_event_loop_unsubscribe(instance->event_loop, instance->event_queue);
    furi_message_queue_free(instance->event_queue);

    scene_manager_free(instance->scene_manager);
    furi_event_loop_free(instance->event_loop);
    free(instance);
}

int32_t widget_gallery_app(void* arg) {
    UNUSED(arg);

    WidgetGallery* instance = widget_gallery_alloc();
    furi_event_loop_run(instance->event_loop);
    widget_gallery_free(instance);

    return 0;
}
