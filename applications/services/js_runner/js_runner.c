#include "js_runner_i.h"
#include "js_fetch.h"

#define TAG "JsRunner"

size_t js_runner_context_alloc(size_t context_size) {
    size_t alloc_size = 0;
    WITH_APP(app, {
        furi_check(!app->jrs_context);
        alloc_size = context_size + app->heap_size;
        app->jrs_context = malloc(alloc_size);
    });
    return alloc_size;
}

void js_runner_context_free(void) {
    WITH_APP(app, {
        furi_check(app->jrs_context);
        free(app->jrs_context);
        app->jrs_context = NULL;
    });
}

void* js_runner_context_get(void) {
    void* result = NULL;
    WITH_APP(app, {
        furi_check(app->jrs_context);
        result = app->jrs_context;
    });
    return result;
}

void js_runner_get_root_path(FuriString* path) {
    WITH_APP(app, { furi_string_set(path, app->root_path); });
}

static const jerry_object_native_info_t global_native_info = {
    .free_cb = NULL,
};

static bool app_has_background_tasks(JsRunnerApp* app) {
    return !IntervalDict_empty_p(app->intervals) || app->num_fetch_threads > 0;
}

void js_runner_check_event_loop(JsRunnerApp* app) {
    if(!app_has_background_tasks(app)) {
        furi_event_loop_stop(app->event_loop);
    }
}

void js_runner_run_jobs(void) {
    FURI_LOG_D(TAG, "run jobs");
    bool run = true;
    while(run) {
        jerry_value_t jobs_result = jerry_run_jobs();
        if(jerry_value_is_exception(jobs_result)) {
            FURI_LOG_E(TAG, "Exception when running jobs");
            // TODO abort event loop
            run = false;
        } else {
            run = false;
        }
        jerry_value_free(jobs_result);
    }
}

static void log_exception(const char* msg, jerry_value_t exception);

void js_runner_check_and_free(jerry_value_t val) {
    if(jerry_value_is_exception(val)) {
        log_exception("check_and_free:", val);
    }
    furi_check(!jerry_value_is_exception(val));
    jerry_value_free(val);
}

static void log_exception(const char* msg, jerry_value_t exception) {
    jerry_value_t val = jerry_exception_value(exception, false);
    jerry_value_t str = jerry_value_to_string(val);
    if(jerry_value_is_string(str)) {
        char buf[64];
        jerry_size_t size =
            jerry_string_to_buffer(str, JERRY_ENCODING_UTF8, (jerry_char_t*)buf, sizeof(buf));
        FURI_LOG_E(TAG, "%s: %.*s", msg, (int)size, buf);
    } else {
        FURI_LOG_E(TAG, "%s: XXXX (not a string)", msg);
    }
    jerry_value_free(str);
    jerry_value_free(val);
}

static void fetch_event_queue_callback(FuriEventLoopObject* object, void* context) {
    UNUSED(object);
    JsRunnerApp* app = context;
    FetchEvent event;
    furi_check(furi_message_queue_get(app->fetch_event_queue, &event, 0) == FuriStatusOk);
    js_fetch_process_event(&event);
    js_runner_run_jobs();
}

static void js_runner_app_init(
    JsRunnerApp* app,
    const char* script_path,
    size_t heap_size,
    JsRunnerConsoleWriteCallback console_write_cb) {
    app->heap_size = heap_size;
    app->jrs_context = NULL;
    app->event_loop = furi_event_loop_alloc();
    app->console_callback = console_write_cb;
    app->root_path = furi_string_alloc();
    app->last_interval_id = 0;
    IntervalDict_init(app->intervals);
    app->num_fetch_threads = 0;
    path_extract_dirname(script_path, app->root_path);
    app->fetch_event_queue = furi_message_queue_alloc(MAX_FETCH_MESSAGES, sizeof(FetchEvent));
    furi_event_loop_subscribe_message_queue(
        app->event_loop,
        app->fetch_event_queue,
        FuriEventLoopEventIn,
        fetch_event_queue_callback,
        app);
}

static void js_runner_app_destroy(JsRunnerApp* app) {
    furi_event_loop_unsubscribe(app->event_loop, app->fetch_event_queue);
    furi_message_queue_free(app->fetch_event_queue);
    furi_event_loop_free(app->event_loop);
    IntervalDict_clear(app->intervals);
    furi_check(app->num_fetch_threads == 0);
    furi_string_free(app->root_path);
}

static void arraybuffer_free_callback(
    jerry_arraybuffer_type_t buffer_type,
    uint8_t* buffer_p,
    uint32_t buffer_size,
    void* arraybuffer_user_p,
    void* user_p) {
    UNUSED(buffer_type);
    UNUSED(buffer_size);
    UNUSED(arraybuffer_user_p);
    UNUSED(user_p);
    FURI_LOG_D(TAG, "Free arraybuffer");
    free(buffer_p);
}

JsRunnerError js_runner_run(
    JsRunner* instance,
    const char* path,
    size_t heap_size,
    JsRunnerConsoleWriteCallback console_write_cb,
    void* console_write_context) {
    FURI_LOG_I(TAG, "Running script: %s", path);

    JsRunnerError ret = JsRunnerErrorNone;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(storage);
    do {
        if(!storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
            ret = JsRunnerErrorCannotOpenFile;
            break;
        }
        uint64_t file_size = storage_file_size(f);
        char* buf = malloc(file_size);
        if(storage_file_read(f, buf, file_size) != file_size) {
            ret = JsRunnerErrorCannotOpenFile;
            free(buf);
            break;
        }

        JsRunnerApp app;
        js_runner_app_init(&app, path, heap_size, console_write_cb);

        {
            furi_mutex_acquire(instance->apps_mutex, FuriWaitForever);
            AppDict_set_at(instance->apps, furi_thread_get_current(), &app);
            furi_mutex_release(instance->apps_mutex);
        }

        jerry_init(JERRY_INIT_EMPTY);
        jerry_arraybuffer_allocator(NULL, arraybuffer_free_callback, NULL);

        {
            jerry_value_t global_obj = jerry_current_realm();
            jerry_object_set_native_ptr(global_obj, &global_native_info, instance);
            jerry_value_free(global_obj);
        }
        js_runner_setup_console(console_write_cb, console_write_context);
        js_runner_setup_interval_methods();
        js_runner_setup_fetch();

        FuriString* path_furi = furi_string_alloc_set_str(path);
        FuriString* filename_furi = furi_string_alloc();
        path_extract_filename(path_furi, filename_furi, false);
        jerry_value_t source_name = jerry_string_sz(furi_string_get_cstr(filename_furi));
        furi_string_free(filename_furi);
        furi_string_free(path_furi);

        jerry_parse_options_t parse_options = {
            .options = JERRY_PARSE_HAS_SOURCE_NAME | JERRY_PARSE_MODULE,
            .source_name = source_name,
        };

        jerry_value_t parsed_script =
            jerry_parse((const jerry_char_t*)buf, file_size, &parse_options);
        free(buf);
        do {
            if(jerry_value_is_exception(parsed_script)) {
                log_exception("Error parsing script", parsed_script);
                ret = JsRunnerParseException;
                break;
            } else {
                js_runner_check_and_free(jerry_module_link(parsed_script, NULL, NULL));
                jerry_value_t result = jerry_module_evaluate(parsed_script);
                if(jerry_value_is_exception(result)) {
                    log_exception("Error running script", result);
                } else {
                    if(app_has_background_tasks(&app)) {
                        furi_event_loop_run(app.event_loop);
                    }
                }
                jerry_value_free(result);
            }
        } while(false);
        jerry_value_free(parsed_script);
        jerry_value_free(source_name);
        jerry_cleanup();

        {
            furi_mutex_acquire(instance->apps_mutex, FuriWaitForever);
            AppDict_erase(instance->apps, furi_thread_get_current());
            furi_mutex_release(instance->apps_mutex);
        }

        js_runner_app_destroy(&app);
    } while(false);
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
    return ret;
}

static JsRunner* js_runner_alloc(void) {
    JsRunner* instance = malloc(sizeof(JsRunner));
    instance->event_loop = furi_event_loop_alloc();
    instance->apps_mutex = furi_mutex_alloc(FuriMutexTypeRecursive);
    AppDict_init(instance->apps);
    furi_record_create(RECORD_JS_RUNNER, instance);
    return instance;
}

int32_t js_runner_srv(void* p) {
    UNUSED(p);

    FURI_LOG_I(TAG, "Service starting...");

    JsRunner* instance = js_runner_alloc();
    furi_event_loop_run(instance->event_loop);

    return 0;
}
