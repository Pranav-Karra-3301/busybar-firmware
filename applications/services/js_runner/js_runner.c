#include "js_runner.h"
#include <furi/furi.h>
#include <storage/storage.h>

#include <m-dict.h>
#include <jerryscript/jerryscript.h>

#define TAG "JsRunner"

typedef enum AppStatus {
    AppStatusIdle,
    AppStatusRunning,
    AppStatusJoined,
} AppStatus;

typedef struct JsRunnerApp {
    size_t heap_size;
    void* jrs_context;
    JsRunnerConsoleWriteCallback console_callback;
} JsRunnerApp;

static size_t app_dict_key_hash(const FuriThread* t) {
    return (size_t)t;
}

M_DICT_DEF2(
    AppDict,
    FuriThread*,
    M_OPEXTEND(M_PTR_OPLIST, HASH(app_dict_key_hash)),
    JsRunnerApp,
    M_POD_OPLIST);

typedef struct JsRunner {
    FuriEventLoop* event_loop;
    FuriMessageQueue* message_queue;

    FuriMutex* apps_mutex;
    AppDict_t apps;
} JsRunner;

size_t js_runner_context_alloc(JsRunner* instance, size_t context_size) {
    furi_mutex_acquire(instance->apps_mutex, FuriWaitForever);
    FuriThread* current_thread = furi_thread_get_current();
    JsRunnerApp* app = AppDict_get(instance->apps, current_thread);
    size_t alloc_size = 0;
    if(app) {
        furi_check(!app->jrs_context);
        alloc_size = context_size + app->heap_size;
        app->jrs_context = malloc(alloc_size);
    } else {
        FURI_LOG_E(TAG, "No JS app handle for current thread");
        furi_crash();
    }

    furi_mutex_release(instance->apps_mutex);
    return alloc_size;
}

void js_runner_context_free(JsRunner* instance) {
    furi_mutex_acquire(instance->apps_mutex, FuriWaitForever);
    FuriThread* current_thread = furi_thread_get_current();
    JsRunnerApp* app = AppDict_get(instance->apps, current_thread);
    if(app) {
        furi_check(app->jrs_context);
        free(app->jrs_context);
        app->jrs_context = NULL;
    } else {
        FURI_LOG_E(TAG, "No JS app handle for current thread");
        furi_crash();
    }
    furi_mutex_release(instance->apps_mutex);
}

void* js_runner_context_get(JsRunner* instance) {
    furi_mutex_acquire(instance->apps_mutex, FuriWaitForever);
    FuriThread* current_thread = furi_thread_get_current();
    const JsRunnerApp* app = AppDict_cget(instance->apps, current_thread);
    void* result = NULL;
    if(app) {
        furi_check(app->jrs_context);
        result = app->jrs_context;
    } else {
        FURI_LOG_E(TAG, "No JS app handle for current thread");
        furi_crash();
    }

    furi_mutex_release(instance->apps_mutex);
    return result;
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

    jerry_value_t name_val = jerry_string_sz(name);
    jerry_object_set(console_obj, name_val, fn);
    jerry_value_free(name_val);
    jerry_value_free(fn);
}

static void
    setup_console(JsRunnerConsoleWriteCallback console_callback, void* console_write_context) {
    jerry_value_t global_obj = jerry_current_realm();

    jerry_value_t console_obj = jerry_object();
    jerry_value_t console_name_val = jerry_string_sz("console");
    jerry_object_set(global_obj, console_name_val, console_obj);

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

    jerry_value_free(console_name_val);
    jerry_value_free(console_obj);
    jerry_value_free(global_obj);
}

JsRunnerError js_runner_run(
    JsRunner* instance,
    const char* filename,
    size_t heap_size,
    JsRunnerConsoleWriteCallback console_write_cb,
    void* console_write_context) {
    FURI_LOG_I(TAG, "Running script: %s", filename);

    JsRunnerError ret = JsRunnerErrorNone;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(storage);
    do {
        if(!storage_file_open(f, filename, FSAM_READ, FSOM_OPEN_EXISTING)) {
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
            .console_callback = console_write_cb,
        };
        {
            furi_mutex_acquire(instance->apps_mutex, FuriWaitForever);
            AppDict_set_at(instance->apps, furi_thread_get_current(), app);
            furi_mutex_release(instance->apps_mutex);
        }

        jerry_init(JERRY_INIT_EMPTY);

        setup_console(console_write_cb, console_write_context);

        jerry_parse_options_t parse_options = {
            .options = JERRY_PARSE_NO_OPTS,
        };

        jerry_value_t parsed_script =
            jerry_parse((const jerry_char_t*)buf, file_size, &parse_options);
        do {
            if(jerry_value_is_exception(parsed_script)) {
                FURI_LOG_E(TAG, "Error parsing script");
                ret = JsRunnerParseException;
                break;
            } else {
                jerry_value_free(jerry_run(parsed_script));
            }
        } while(false);
        jerry_value_free(parsed_script);
        jerry_cleanup();

        {
            furi_mutex_acquire(instance->apps_mutex, FuriWaitForever);
            AppDict_erase(instance->apps, furi_thread_get_current());
            furi_mutex_release(instance->apps_mutex);
        }
    } while(false);
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
    return ret;
}

static JsRunner* js_runner_alloc(void) {
    JsRunner* instance = malloc(sizeof(JsRunner));
    instance->event_loop = furi_event_loop_alloc();
    instance->apps_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
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
