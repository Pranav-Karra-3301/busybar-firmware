#include <furi.h>

#include <audio/audio.h>
#include <storage/storage.h>
#include <gui/gui.h>
#include <gui/modules/label.h>

#include <jerryscript.h>

#define TAG "JsGui"

typedef enum {
    JsGuiCustomEventExit = 1UL << 0,
} JsGuiCustomEvent;

typedef struct {
    FuriEventLoop* event_loop;
    Audio* audio;
    Gui* gui;

    jerry_value_t app_obj;
    jerry_value_t gui_obj;
} JsGui;

static const char* script = "class JsGui {"
                            "   name;"
                            "   constructor(name) {"
                            "        this.name = name;"
                            "   }"
                            "   onDraw(gui) {"
                            "       this.label = gui.label(this.name);"
                            "   }"
                            "}"
                            "new JsGui(\"Hello JS\");";

static JsGui* jsgui = NULL;

typedef struct label_context_t {
    Label* labels[2];
} label_context_t;

static void js_label_free(void* context, jerry_object_native_info_t* info) {
    UNUSED(info);
    label_context_t* instance = context;
    with_gui(jsgui->gui, {
        label_free(instance->labels[0]);
        label_free(instance->labels[1]);
    });
    free(instance);

    FURI_LOG_D(TAG, "Label deleted");
}

static const jerry_object_native_info_t label_type_info = {.free_cb = js_label_free};

static bool input_callback(const InputEvent* event, void* context) {
    furi_assert(event);
    furi_assert(context);
    UNUSED(script);
    JsGui* instance = context;
    UNUSED(instance);

    return false;
}

static void custom_event_callback(uint32_t events, void* context) {
    furi_assert(context);
    JsGui* instance = context;

    if(events & JsGuiCustomEventExit) {
        furi_event_loop_stop(instance->event_loop);
    }
}

static jerry_value_t gui_label_handler(
    const jerry_call_info_t* call_info_p,
    const jerry_value_t args_p[],
    const jerry_length_t args_cnt) {
    UNUSED(call_info_p);
    if(args_cnt == 1 && jerry_value_is_string(args_p[0])) {
        size_t buffer_size = jerry_string_length(args_p[0]) + 1;
        jerry_char_t* buffer = malloc(buffer_size);
        jerry_string_to_buffer(args_p[0], JERRY_ENCODING_UTF8, buffer, buffer_size);

        label_context_t* label_context = malloc(sizeof(label_context_t));

        with_gui(jsgui->gui, {
            GuiLayer* main_layer = gui_get_layer(jsgui->gui, GuiLayerIdMain);

            for(GuiDisplayId id = 0; id < GuiDisplayIdMax; ++id) {
                Widget* root = gui_layer_get_root_widget(main_layer, id);

                Label* label = label_alloc(root);
                label_set_text(label, (char*)buffer);
                widget_set_align(label_get_base(label), AlignCenter);

                label_context->labels[id] = label;
            }
        });
        jerry_value_t obj = jerry_object();
        jerry_object_set_native_ptr(obj, &label_type_info, label_context);
        free(buffer);
        return obj;
    } else {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "Invalid argument count");
        // return jerry_undefined();
    }
}

static jerry_value_t create_gui_object(void) {
    jerry_value_t obj = jerry_object();
    jerry_value_t gui_label_method = jerry_function_external(gui_label_handler);
    jerry_value_t prop_name = jerry_string_sz("label");

    jerry_value_free(jerry_object_set(obj, prop_name, gui_label_method));
    jerry_value_free(prop_name);
    jerry_value_free(gui_label_method);
    return obj;
}

static void draw(JsGui* instance) {
    jerry_value_t on_draw_str = jerry_string_sz("onDraw");
    jerry_value_t on_draw_fn = jerry_object_get(instance->app_obj, on_draw_str);
    jerry_value_free(on_draw_str);

    furi_check(jerry_value_is_function(on_draw_fn));

    jerry_value_t ret = jerry_call(on_draw_fn, instance->app_obj, &instance->gui_obj, 1);

    jerry_value_free(on_draw_fn);

    if(jerry_value_is_exception(ret)) {
        FURI_LOG_E(TAG, "Exception when calling app.");
    }
    jerry_value_free(ret);
}

static JsGui* js_gui_alloc(void) {
    jerry_init(JERRY_INIT_EMPTY);
    JsGui* instance = malloc(sizeof(JsGui));
    instance->event_loop = furi_event_loop_alloc();
    instance->audio = furi_record_open(RECORD_AUDIO);
    instance->gui = furi_record_open(RECORD_GUI);

    furi_event_loop_set_custom_event_callback(
        instance->event_loop, custom_event_callback, instance);

    jsgui = instance;

    with_gui(instance->gui, {
        GuiLayer* main_layer = gui_get_layer(instance->gui, GuiLayerIdMain);
        gui_layer_add_input_callback(main_layer, input_callback, instance);
    });

    instance->app_obj =
        jerry_eval((const jerry_char_t*)script, strlen(script), JERRY_PARSE_NO_OPTS);

    if(!jerry_value_is_object(instance->app_obj)) {
        FURI_LOG_E(TAG, "App is not an object");
        furi_crash();
    }

    instance->gui_obj = create_gui_object();

    furi_check(jerry_value_is_object(instance->gui_obj));

    draw(instance);

    return instance;
}

static void js_gui_free(JsGui* instance) {
    FURI_LOG_D(TAG, "js_gui_free");
    with_gui(instance->gui, {
        GuiLayer* main_layer = gui_get_layer(instance->gui, GuiLayerIdMain);
        gui_layer_remove_input_callback(main_layer, input_callback);
    });

    jerry_value_free(instance->app_obj);
    jerry_value_free(instance->gui_obj);

    jerry_cleanup();

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_AUDIO);

    furi_event_loop_free(instance->event_loop);
    FURI_LOG_D(TAG, "instance free");
    free(instance);
    jsgui = NULL;
}

int32_t js_gui_app(void* arg) {
    UNUSED(arg);
    JsGui* instance = js_gui_alloc();
    furi_event_loop_run(instance->event_loop);
    js_gui_free(instance);

    return 0;
}
