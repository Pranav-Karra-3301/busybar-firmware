#include "js_runner.h"
#include <furi/furi.h>
#include <storage/storage.h>
#include <path.h>

#include <m-dict.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
#include <jerryscript.h>
#pragma GCC diagnostic pop

#define TAG "JsRunner"

#define MIN_INTERVAL_DELAY_MS 10.0f

typedef enum AppStatus {
    AppStatusIdle,
    AppStatusRunning,
    AppStatusJoined,
} AppStatus;

typedef struct IntervalContext {
    bool once;
    FuriEventLoopTimer* timer;
    jerry_value_t callback;
} IntervalContext;

M_DICT_DEF2(IntervalDict, uint32_t, M_DEFAULT_OPLIST, IntervalContext, M_POD_OPLIST);

typedef struct JsRunnerApp {
    size_t heap_size;
    void* jrs_context;
    FuriEventLoop* event_loop;
    JsRunnerConsoleWriteCallback console_callback;
    FuriString* root_path;
    IntervalDict_t intervals;
    uint32_t last_interval_id;
} JsRunnerApp;

static size_t app_dict_key_hash(const FuriThread* t) {
    return (size_t)t;
}

M_DICT_DEF2(
    AppDict,
    FuriThread*,
    M_OPEXTEND(M_PTR_OPLIST, HASH(app_dict_key_hash)),
    JsRunnerApp*,
    M_PTR_OPLIST);

typedef struct JsRunner {
    FuriEventLoop* event_loop;
    FuriMessageQueue* message_queue;

    FuriMutex* apps_mutex;
    AppDict_t apps;
} JsRunner;

#define WITH_APP(APP, BLOCK)                                             \
    do {                                                                 \
        furi_mutex_acquire(instance->apps_mutex, FuriWaitForever);       \
        FuriThread* current_thread = furi_thread_get_current();          \
        JsRunnerApp* APP = *AppDict_get(instance->apps, current_thread); \
        if(APP) {                                                        \
            BLOCK                                                        \
        } else {                                                         \
            FURI_LOG_E(TAG, "No JS app handle for current thread");      \
            furi_crash();                                                \
        }                                                                \
        furi_mutex_release(instance->apps_mutex);                        \
    } while(false)

size_t js_runner_context_alloc(JsRunner* instance, size_t context_size) {
    size_t alloc_size = 0;
    WITH_APP(app, {
        furi_check(!app->jrs_context);
        alloc_size = context_size + app->heap_size;
        app->jrs_context = malloc(alloc_size);
    });
    return alloc_size;
}

void js_runner_context_free(JsRunner* instance) {
    WITH_APP(app, {
        furi_check(app->jrs_context);
        free(app->jrs_context);
        app->jrs_context = NULL;
    });
}

void* js_runner_context_get(JsRunner* instance) {
    void* result = NULL;
    WITH_APP(app, {
        furi_check(app->jrs_context);
        result = app->jrs_context;
    });
    return result;
}

void js_runner_get_root_path(JsRunner* instance, FuriString* path) {
    WITH_APP(app, { furi_string_set(path, app->root_path); });
}

static const jerry_object_native_info_t global_native_info = {
    .free_cb = NULL,
};

static JsRunner* get_instance(void) {
    jerry_value_t global_obj = jerry_current_realm();
    JsRunner* instance = jerry_object_get_native_ptr(global_obj, &global_native_info);
    furi_check(instance);
    jerry_value_free(global_obj);
    return instance;
}

static bool app_has_background_tasks(JsRunnerApp* app) {
    return !IntervalDict_empty_p(app->intervals);
}

static void check_event_loop(JsRunnerApp* app) {
    if(!app_has_background_tasks(app)) {
        furi_event_loop_stop(app->event_loop);
    }
}

typedef struct {
    JsRunnerConsoleWriteCallback write;
    void* context;
    JsRunnerConsoleSeverity severity;
} ConsoleContext;

static void console_context_free(void* native_p, jerry_object_native_info_t* info_p) {
    UNUSED(info_p);
    free(native_p);
}

static const jerry_object_native_info_t console_native_info = {
    .free_cb = console_context_free,
};

static jerry_value_t console_log(
    const jerry_call_info_t* call_info,
    const jerry_value_t args[],
    const jerry_length_t args_count) {
    ConsoleContext* ctx = jerry_object_get_native_ptr(call_info->function, &console_native_info);
    JsRunnerConsoleSeverity severity = ctx->severity;

    furi_check(ctx);

    if(ctx->write == NULL) {
        return jerry_undefined();
    }

    for(jerry_length_t i = 0; i < args_count; i++) {
        jerry_value_t str = jerry_value_to_string(args[i]);

        jerry_size_t size = jerry_string_size(str, JERRY_ENCODING_UTF8);

        char* buf = malloc(size);

        jerry_string_to_buffer(str, JERRY_ENCODING_UTF8, (jerry_char_t*)buf, size);

        ctx->write(severity, buf, size, ctx->context);
        free(buf);

        if(i + 1 != args_count) ctx->write(severity, " ", 1, ctx->context);

        jerry_value_free(str);
    }

    ctx->write(severity, "\n", 1, ctx->context);

    return jerry_undefined();
}

static void interval_callback(void* context) {
    uint32_t timer_id = (uint32_t)context;
    JsRunner* instance = get_instance();

    FURI_LOG_D(TAG, "Interval callback (id=%lu)", timer_id);

    WITH_APP(app, {
        const IntervalContext* interval_context = IntervalDict_cget(app->intervals, timer_id);
        if(interval_context) {
            jerry_value_free(jerry_call(interval_context->callback, jerry_undefined(), NULL, 0));
        } else {
            FURI_LOG_E(TAG, "Dead interval timer with id = %lu", timer_id);
        }

        interval_context = IntervalDict_cget(app->intervals, timer_id);
        if(interval_context && interval_context->once) {
            jerry_value_free(interval_context->callback);
            furi_event_loop_timer_free(interval_context->timer);
            IntervalDict_erase(app->intervals, timer_id);
            check_event_loop(app);
        }
    });
}

static jerry_value_t set_interval_or_timeout(
    const jerry_value_t args[],
    const jerry_length_t args_count,
    bool once) {
    JsRunner* instance = get_instance();

    if(args_count != 2) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "Wrong argument count");
    }

    jerry_value_t callback_val = args[0];
    float timeout_ms = jerry_value_as_number(args[1]);
    if(timeout_ms < MIN_INTERVAL_DELAY_MS) {
        timeout_ms = MIN_INTERVAL_DELAY_MS;
    }

    FURI_LOG_D(TAG, "set_interval timeout = %d, once = %d", (int)timeout_ms, once);

    uint32_t timer_id = 0;
    WITH_APP(app, {
        timer_id = app->last_interval_id;
        app->last_interval_id += 1;
    });

    IntervalContext interval_context = {
        .callback = jerry_value_copy(callback_val),
        .once = once,
    };
    WITH_APP(app, {
        interval_context.timer = furi_event_loop_timer_alloc(
            app->event_loop,
            interval_callback,
            once ? FuriEventLoopTimerTypeOnce : FuriEventLoopTimerTypePeriodic,
            (void*)timer_id);
        IntervalDict_set_at(app->intervals, timer_id, interval_context);
        furi_event_loop_timer_start(
            interval_context.timer, furi_ms_to_ticks((uint32_t)timeout_ms));
    });

    return jerry_number(timer_id);
}

static jerry_value_t set_interval(
    const jerry_call_info_t* call_info,
    const jerry_value_t args[],
    const jerry_length_t args_count) {
    UNUSED(call_info);

    return set_interval_or_timeout(args, args_count, false);
}

static jerry_value_t set_timeout(
    const jerry_call_info_t* call_info,
    const jerry_value_t args[],
    const jerry_length_t args_count) {
    UNUSED(call_info);
    return set_interval_or_timeout(args, args_count, true);
}

static jerry_value_t clear_interval(
    const jerry_call_info_t* call_info,
    const jerry_value_t args[],
    const jerry_length_t args_count) {
    UNUSED(call_info);

    JsRunner* instance = get_instance();

    if(args_count != 1) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "Wrong argument count");
    }
    uint32_t timer_id = (uint32_t)jerry_value_as_number(args[0]);
    FURI_LOG_D(TAG, "clear_interval id = %lu", timer_id);

    WITH_APP(app, {
        const IntervalContext* interval_context = IntervalDict_cget(app->intervals, timer_id);
        if(interval_context) {
            jerry_value_free(interval_context->callback);
            furi_event_loop_timer_free(interval_context->timer);
            IntervalDict_erase(app->intervals, timer_id);
        } else {
            FURI_LOG_E(TAG, "Interval with ID %lu is not found", timer_id);
        }
        check_event_loop(app);
    });
    return jerry_undefined();
}

static void check_and_free(jerry_value_t val) {
    furi_check(!jerry_value_is_exception(val));
    jerry_value_free(val);
}

static void add_logging_method(
    jerry_value_t console_obj,
    const char* name,
    JsRunnerConsoleSeverity severity,
    JsRunnerConsoleWriteCallback console_callback,
    void* console_write_context) {
    ConsoleContext* fn_context = malloc(sizeof(ConsoleContext));
    fn_context->write = console_callback;
    fn_context->context = console_write_context;
    fn_context->severity = severity;

    jerry_value_t fn = jerry_function_external(console_log);
    jerry_object_set_native_ptr(fn, &console_native_info, fn_context);

    check_and_free(jerry_object_set_sz(console_obj, name, fn));
    jerry_value_free(fn);
}

static void
    setup_console(JsRunnerConsoleWriteCallback console_callback, void* console_write_context) {
    jerry_value_t global_obj = jerry_current_realm();

    jerry_value_t console_obj = jerry_object();
    check_and_free(jerry_object_set_sz(global_obj, "console", console_obj));

    add_logging_method(
        console_obj, "log", JsRunnerConsoleSeverityLog, console_callback, console_write_context);
    add_logging_method(
        console_obj, "info", JsRunnerConsoleSeverityInfo, console_callback, console_write_context);
    add_logging_method(
        console_obj,
        "error",
        JsRunnerConsoleSeverityError,
        console_callback,
        console_write_context);

    jerry_value_free(console_obj);
    jerry_value_free(global_obj);
}

static void setup_interval_methods(void) {
    jerry_value_t global_obj = jerry_current_realm();

    jerry_value_t set_interval_fn = jerry_function_external(set_interval);
    jerry_value_t set_timeout_fn = jerry_function_external(set_timeout);
    jerry_value_t clear_interval_fn = jerry_function_external(clear_interval);

    check_and_free(jerry_object_set_sz(global_obj, "setInterval", set_interval_fn));
    check_and_free(jerry_object_set_sz(global_obj, "setTimeout", set_timeout_fn));
    check_and_free(jerry_object_set_sz(global_obj, "clearInterval", clear_interval_fn));
    check_and_free(jerry_object_set_sz(global_obj, "clearTimeout", clear_interval_fn));

    jerry_value_free(set_interval_fn);
    jerry_value_free(set_timeout_fn);
    jerry_value_free(clear_interval_fn);
    jerry_value_free(global_obj);
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
        if(!storage_file_read(f, buf, file_size)) {
            ret = JsRunnerErrorCannotOpenFile;
            free(buf);
            break;
        }

        JsRunnerApp app = {
            .heap_size = heap_size,
            .jrs_context = NULL,
            .event_loop = furi_event_loop_alloc(),
            .console_callback = console_write_cb,
            .root_path = furi_string_alloc(),
            .last_interval_id = 0,
        };
        IntervalDict_init(app.intervals);
        path_extract_dirname(path, app.root_path);

        {
            furi_mutex_acquire(instance->apps_mutex, FuriWaitForever);
            AppDict_set_at(instance->apps, furi_thread_get_current(), &app);
            furi_mutex_release(instance->apps_mutex);
        }

        jerry_init(JERRY_INIT_EMPTY);

        {
            jerry_value_t global_obj = jerry_current_realm();
            jerry_object_set_native_ptr(global_obj, &global_native_info, instance);
            jerry_value_free(global_obj);
        }
        setup_console(console_write_cb, console_write_context);
        setup_interval_methods();

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
                jerry_value_free(jerry_module_link(parsed_script, NULL, NULL));
                jerry_value_t result = jerry_module_evaluate(parsed_script);
                if(jerry_value_is_exception(result)) {
                    log_exception("Error running script", parsed_script);
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

        furi_event_loop_free(app.event_loop);
        IntervalDict_clear(app.intervals);
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
