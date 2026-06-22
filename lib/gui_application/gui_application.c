#include "gui_application_i.h"

#define EVENT_QUEUE_SIZE (8)
#define INPUT_QUEUE_SIZE (8)

static bool gui_application_input_callback(const InputEvent* event, void* context) {
    furi_assert(event);
    furi_assert(context);

    bool consumed = false;

    GuiApplication* instance = context;

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

static void gui_application_input_queue_callback(FuriEventLoopObject* object, void* context) {
    furi_assert(context);

    GuiApplication* instance = context;
    furi_assert(instance->input_queue == object);

    InputEvent event;
    while(furi_message_queue_get(instance->input_queue, &event, 0) == FuriStatusOk) {
        if(event.type == InputTypeShort) {
            if(event.key == InputKeyBack) {
                scene_manager_handle_back_event(instance->scene_manager);
            }
        }
    }
}

static void gui_application_event_queue_callback(FuriEventLoopObject* object, void* context) {
    furi_assert(context);

    GuiApplication* instance = context;
    furi_assert(instance->event_queue == object);

    uint32_t event;
    while(furi_message_queue_get(instance->event_queue, &event, 0) == FuriStatusOk) {
        scene_manager_handle_custom_event(instance->scene_manager, event);
    }
}

void gui_application_init(GuiApplication* instance, const GuiApplicationConfig* config) {
    instance->event_loop = furi_event_loop_alloc();
    instance->event_queue = furi_message_queue_alloc(EVENT_QUEUE_SIZE, sizeof(SceneManagerEvent));
    instance->input_queue = furi_message_queue_alloc(INPUT_QUEUE_SIZE, sizeof(InputEvent));
    instance->scene_manager = scene_manager_alloc(config->scenes, config->scenes_count, instance);
    instance->gui = furi_record_open(RECORD_GUI);

    furi_event_loop_subscribe_message_queue(
        instance->event_loop,
        instance->event_queue,
        FuriEventLoopEventIn,
        gui_application_event_queue_callback,
        instance);

    furi_event_loop_subscribe_message_queue(
        instance->event_loop,
        instance->input_queue,
        FuriEventLoopEventIn,
        gui_application_input_queue_callback,
        instance);

    with_gui(instance->gui, {
        GuiLayer* layer = gui_get_layer(instance->gui, GuiLayerIdMain);
        gui_layer_add_input_callback(layer, gui_application_input_callback, instance);

        for(GuiDisplayId id = 0; id < GuiDisplayIdMax; ++id) {
            Widget* root = gui_layer_get_root_widget(layer, id);
            instance->windows[id] = widget_alloc(root);
        }
    });
}

void gui_application_clear(GuiApplication* instance) {
    with_gui(instance->gui, {
        GuiLayer* layer = gui_get_layer(instance->gui, GuiLayerIdMain);
        gui_layer_remove_input_callback(layer, gui_application_input_callback);

        for(GuiDisplayId id = 0; id < GuiDisplayIdMax; ++id) {
            widget_free(instance->windows[id]);
        }
    });

    furi_event_loop_unsubscribe(instance->event_loop, instance->event_queue);
    furi_event_loop_unsubscribe(instance->event_loop, instance->input_queue);

    furi_message_queue_free(instance->event_queue);
    furi_message_queue_free(instance->input_queue);
    furi_event_loop_free(instance->event_loop);
    scene_manager_free(instance->scene_manager);

    furi_record_close(RECORD_GUI);
}

GuiApplication* gui_application_alloc(const GuiApplicationConfig* config) {
    furi_check(config);

    GuiApplication* instance = malloc(sizeof(GuiApplication));
    gui_application_init(instance, config);
    return instance;
}

void gui_application_free(GuiApplication* instance) {
    furi_check(instance);
    gui_application_clear(instance);
    free(instance);
}

void gui_application_run(GuiApplication* instance) {
    furi_check(instance);
    furi_event_loop_run(instance->event_loop);
}

void gui_application_send_custom_event(GuiApplication* instance, uint32_t event) {
    furi_check(instance);

    const SceneManagerEvent queued_event = {
        .type = SceneManagerEventTypeCustom,
        .event = event,
    };

    furi_check(
        furi_message_queue_put(instance->event_queue, &queued_event, FuriWaitForever) ==
        FuriStatusOk);
}
