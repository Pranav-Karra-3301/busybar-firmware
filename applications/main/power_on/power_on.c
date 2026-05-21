#include "power_on_i.h"
#include <storage/storage.h>
// #include <furi.h>

// // #include <busy_timer/time_macros.h>

// #define POWER_ON_START_TIMEOUT_MS (500)
// #define POWER_ON_APP_TIMEOUT_MIN  (15)

#define POWER_ON_DONE_PATH APP_DATA_PATH("done.txt")

// static const char* const power_on_anim_paths[GuiDisplayIdMax] = {
//     POWER_ON_ANIM_PATH("front_power_on_72x16.anim"),
//     POWER_ON_ANIM_PATH("back_power_on_148x80.anim"),
// };

// static bool power_on_input_callback(const InputEvent* event, void* context) {
//     furi_assert(event);
//     furi_assert(context);
//     PowerOnApp* instance = context;

//     bool consumed = false;
//     if(event->type == InputTypeShort) {
//         switch(event->key) {
//         case InputKeyOk:
//         case InputKeyBack:
//         case InputKeyStart:
//         case InputKeyBusy:
//         case InputKeyCustom:
//         case InputKeyOff:
//         case InputKeyApps:
//         case InputKeySettings:
//             furi_thread_flags_set(instance->thread_id, PowerOnAppFlagUserInteracted);
//             consumed = true;
//             break;
//         default:
//             break;
//         }
//     }

//     return consumed;
// }

// static void power_on_shutdown_timer_callback(void* ctx) {
//     PowerOnApp* instance = ctx;
//     furi_thread_flags_set(instance->thread_id, PowerOnAppFlagShutdownRequired);
// }

// static bool power_on_thread_signal_callback(uint32_t signal, void* arg, void* context) {
//     UNUSED(arg);
//     furi_assert(context);

//     PowerOnApp* instance = context;

//     if(signal == FuriSignalExit) {
//         // Desktop has received the initial switch state and wants to close us
//         const uint32_t flags =
//             furi_thread_flags_set(instance->thread_id, PowerOnAppFlagStartupComplete);
//         furi_check((flags & FuriFlagError) == 0);
//         return true;
//     }

//     return false;
// }

bool power_on_is_done_flag_present(PowerOnApp* instance) {
    return storage_file_exists(instance->storage, POWER_ON_DONE_PATH);
}

void power_on_done_flag_create(PowerOnApp* instance) {
    File* file = storage_file_alloc(instance->storage);

    if(!storage_file_open(file, POWER_ON_DONE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FURI_LOG_W(TAG, "Failed to create file");
    }

    storage_file_close(file);
    storage_file_free(file);
}

// static AnimPlayer* power_on_animation_alloc(Widget* widget, GuiDisplayId display_id) {
//     AnimPlayer* anim = anim_player_alloc(widget);

//     if(anim_player_set_source(anim, power_on_anim_paths[display_id])) {
//         anim_player_set_section(anim, POWER_ON_ANIM_FLAGS, POWER_ON_ANIM_SECTION);
//     }

//     return anim;
// }

// static void power_on_show_startup_message(PowerOnApp* instance) {
//     with_gui(instance->gui, {
//         GuiLayer* layer_main = gui_get_layer(instance->gui, GuiLayerIdMain);

//         for(GuiDisplayId id = 0; id < GuiDisplayIdMax; ++id) {
//             Widget* root = gui_layer_get_root_widget(layer_main, id);

//             Label* label = label_alloc(root);
//             label_set_text(label, "Starting...");
//             widget_set_align(label_get_base(label), AlignCenter);

//             instance->labels[id] = label;
//         }
//     });
// }

// static void power_on_show_first_boot_animation(PowerOnApp* instance) {
//     with_gui(instance->gui, {
//         GuiLayer* layer_main = gui_get_layer(instance->gui, GuiLayerIdMain);
//         gui_layer_add_input_callback(layer_main, power_on_input_callback, instance);

//         for(GuiDisplayId id = 0; id < GuiDisplayIdMax; ++id) {
//             Widget* root = gui_layer_get_root_widget(layer_main, id);
//             instance->anims[id] = power_on_animation_alloc(root, id);
//         }
//     });
// }

// static void power_on_wait_for_start_condition(PowerOnApp* instance) {
//     // avoid showing text for < 500ms
//     uint32_t flags;

//     flags = furi_thread_flags_wait(
//         POWER_ON_APP_STARTUP_FLAGS, FuriFlagWaitAny, furi_ms_to_ticks(POWER_ON_START_TIMEOUT_MS));

//     if(flags == FuriFlagErrorTimeout) {
//         power_on_show_startup_message(instance);
//         flags =
//             furi_thread_flags_wait(POWER_ON_APP_STARTUP_FLAGS, FuriFlagWaitAny, FuriWaitForever);
//     }

//     furi_check((flags & FuriFlagError) == 0);
// }

// static void power_on_wait_for_exit_condition(PowerOnApp* instance) {
//     const uint32_t flags =
//         furi_thread_flags_wait(POWER_ON_APP_ANIMATION_FLAGS, FuriFlagWaitAny, FuriWaitForever);

//     if(flags & PowerOnAppFlagShutdownRequired) {
//         power_off(instance->power);
//     }

//     if(flags & PowerOnAppFlagUserInteracted) {
//         furi_timer_stop(instance->shutdown_timer);
//         power_on_done_flag_create(instance);
//     }
// }

// static PowerOnApp* power_on_app_alloc(void) {
//     PowerOnApp* instance = malloc(sizeof(PowerOnApp));

//     instance->gui = furi_record_open(RECORD_GUI);
//     instance->power = furi_record_open(RECORD_POWER);
//     instance->storage = furi_record_open(RECORD_STORAGE);

//     instance->thread_id = furi_thread_get_current_id();
//     instance->shutdown_timer =
//         furi_timer_alloc(power_on_shutdown_timer_callback, FuriTimerTypeOnce, instance);
//     furi_timer_start(
//         instance->shutdown_timer, furi_ms_to_ticks(M_TO_MS(POWER_ON_APP_TIMEOUT_MIN)));

//     furi_thread_set_signal_callback(
//         furi_thread_get_current(), power_on_thread_signal_callback, instance);

//     return instance;
// }

// static void power_on_app_free(PowerOnApp* instance) {
//     furi_timer_free(instance->shutdown_timer);

//     with_gui(instance->gui, {
//         for(GuiDisplayId id = 0; id < GuiDisplayIdMax; ++id) {
//             if(instance->labels[id]) {
//                 label_free(instance->labels[id]);
//             }

//             if(instance->anims[id]) {
//                 anim_player_free(instance->anims[id]);
//             }
//         }

//         GuiLayer* layer_main = gui_get_layer(instance->gui, GuiLayerIdMain);
//         gui_layer_remove_input_callback(layer_main, power_on_input_callback);
//     });

//     furi_record_close(RECORD_STORAGE);
//     furi_record_close(RECORD_POWER);
//     furi_record_close(RECORD_GUI);

//     free(instance);
// }

// int32_t power_on_app(void* arg) {
//     UNUSED(arg);

//     PowerOnApp* instance = power_on_app_alloc();

//     power_on_wait_for_start_condition(instance);

//     if(!power_on_is_done_flag_present(instance)) {
//         power_on_show_first_boot_animation(instance);
//         power_on_wait_for_exit_condition(instance);
//     }

//     power_on_app_free(instance);

//     return 0;
// }

static bool power_on_thread_signal_callback(uint32_t signal, void* arg, void* context) {
    UNUSED(arg);

    PowerOnApp* instance = context;

    switch(signal) {
    case FuriSignalExit:
        // Desktop has received the initial switch state and wants to close us
        power_on_send_custom_event(instance, PowerOnAppEventStarted);
        return true;

    default:
        return false;
    }
}

static void power_on_input_queue_callback(FuriEventLoopObject* object, void* context) {
    UNUSED(object);

    furi_assert(context);

    PowerOnApp* instance = context;

    InputEvent event;
    while(furi_message_queue_get(instance->input_queue, &event, 0) == FuriStatusOk) {
        if(event.type == InputTypeShort) {
            if(event.key == InputKeyBack) {
                scene_manager_handle_back_event(instance->scene_manager);
            }
        }
    }
}

static void power_on_event_queue_callback(FuriEventLoopObject* object, void* context) {
    UNUSED(object);

    furi_assert(context);

    PowerOnApp* instance = context;

    uint32_t event;
    while(furi_message_queue_get(instance->event_queue, &event, 0) == FuriStatusOk) {
        scene_manager_handle_custom_event(instance->scene_manager, event);
    }
}

static bool power_on_gui_input_callback(const InputEvent* event, void* context) {
    furi_assert(event);
    furi_assert(context);

    PowerOnApp* instance = context;

    bool consumed = false;
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

static PowerOnApp* power_on_alloc() {
    PowerOnApp* instance = malloc(sizeof(PowerOnApp));
    instance->event_loop = furi_event_loop_alloc();
    instance->input_queue = furi_message_queue_alloc(4, sizeof(InputEvent));
    instance->event_queue = furi_message_queue_alloc(8, sizeof(uint32_t));
    instance->scene_manager =
        scene_manager_alloc(power_on_scenes, COUNT_OF(power_on_scenes), instance);

    instance->gui = furi_record_open(RECORD_GUI);
    instance->power = furi_record_open(RECORD_POWER);
    instance->storage = furi_record_open(RECORD_STORAGE);

    with_gui(instance->gui, {
        GuiLayer* layer = gui_get_layer(instance->gui, GuiLayerIdMain);
        gui_layer_add_input_callback(layer, power_on_gui_input_callback, instance);

        instance->front_root = gui_layer_get_root_widget(layer, GuiDisplayIdFront);
        instance->back_root = gui_layer_get_root_widget(layer, GuiDisplayIdBack);
    });

    furi_event_loop_subscribe_message_queue(
        instance->event_loop,
        instance->input_queue,
        FuriEventLoopEventIn,
        power_on_input_queue_callback,
        instance);

    furi_event_loop_subscribe_message_queue(
        instance->event_loop,
        instance->event_queue,
        FuriEventLoopEventIn,
        power_on_event_queue_callback,
        instance);

    scene_manager_next_scene(instance->scene_manager, SceneIdStarting);

    return instance;
}

static void power_on_free(PowerOnApp* instance) {
    furi_assert(instance);
    scene_manager_free(instance->scene_manager);

    with_gui(instance->gui, {
        GuiLayer* layer = gui_get_layer(instance->gui, GuiLayerIdMain);
        gui_layer_remove_input_callback(layer, power_on_gui_input_callback);
    });

    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_POWER);
    furi_record_close(RECORD_GUI);

    furi_event_loop_unsubscribe(instance->event_loop, instance->input_queue);
    furi_event_loop_unsubscribe(instance->event_loop, instance->event_queue);
    furi_message_queue_free(instance->input_queue);
    furi_message_queue_free(instance->event_queue);

    furi_event_loop_free(instance->event_loop);
    free(instance);
}

int32_t power_on_app(void* arg) {
    UNUSED(arg);

    PowerOnApp* instance = power_on_alloc();
    FuriThread* thread = furi_thread_get_current();
    furi_thread_set_signal_callback(thread, power_on_thread_signal_callback, instance);
    furi_event_loop_run(instance->event_loop);
    furi_thread_set_signal_callback(thread, NULL, NULL);
    power_on_free(instance);

    return 0;
}

void power_on_send_custom_event(PowerOnApp* instance, uint32_t event) {
    furi_assert(instance);

    furi_check(
        furi_message_queue_put(instance->event_queue, &event, FuriWaitForever) == FuriStatusOk);
}
