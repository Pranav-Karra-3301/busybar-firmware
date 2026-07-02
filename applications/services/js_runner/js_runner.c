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
    AppStatus status;
    size_t script_size;
    char* script;
    size_t heap_size;
    void* jrs_context;
    JsRunnerConsoleWriteCallback console_callback;
} JsRunnerApp;

typedef struct JsRunnerAppHandle {
    JsRunner* instance;
    FuriThread* thread;
} JsRunnerAppHandle;

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
    ConsoleContext* ctx = jerry_object_get_native_ptr(call_info->this_value, &console_native_info);

    furi_check(ctx);

    if(ctx->write == NULL) {
        return jerry_undefined();
    }

    for(jerry_length_t i = 0; i < args_count; i++) {
        jerry_value_t str = jerry_value_to_string(args[i]);

        jerry_size_t size = jerry_string_size(str, JERRY_ENCODING_UTF8);

        char* buf = malloc(size);

        jerry_string_to_buffer(str, JERRY_ENCODING_UTF8, (jerry_char_t*)buf, size);

        ctx->write(buf, size);
        free(buf);

        if(i + 1 != args_count) ctx->write(" ", 1);

        jerry_value_free(str);
    }

    ctx->write("\n", 1);

    return jerry_undefined();
}

static void setup_console(JsRunnerConsoleWriteCallback console_callback) {
    jerry_value_t global = jerry_current_realm();

    ConsoleContext* console_ctx = malloc(sizeof(ConsoleContext));
    console_ctx->write = console_callback;

    jerry_value_t console = jerry_object();
    jerry_object_set_native_ptr(console, &console_native_info, console_ctx);

    jerry_value_t log_fn = jerry_function_external(console_log);
    jerry_value_t console_name = jerry_string_sz("console");
    jerry_value_t log_name = jerry_string_sz("log");

    jerry_object_set(console, log_name, log_fn);

    jerry_object_set(global, console_name, console);

    jerry_value_free(log_name);
    jerry_value_free(console_name);
    jerry_value_free(log_fn);
    jerry_value_free(console);
    jerry_value_free(global);
}

static int32_t js_app_thread_callback(void* context) {
    JsRunnerAppHandle* handle = context;
    JsRunner* instance = handle->instance;
    JsRunnerApp app;
    {
        furi_mutex_acquire(instance->apps_mutex, FuriWaitForever);
        app = *AppDict_cget(instance->apps, handle->thread);
        furi_mutex_release(instance->apps_mutex);
    }
    jerry_init(JERRY_INIT_EMPTY);

    setup_console(app.console_callback);

    jerry_parse_options_t parse_options = {
        .options = JERRY_PARSE_NO_OPTS,
    };

    jerry_value_t parsed_script =
        jerry_parse((const jerry_char_t*)app.script, app.script_size, &parse_options);
    {
        furi_mutex_acquire(instance->apps_mutex, FuriWaitForever);
        JsRunnerApp* app = AppDict_get(instance->apps, handle->thread);
        free(app->script);
        app->script = NULL;
        furi_mutex_release(instance->apps_mutex);
    }
    if(jerry_value_is_exception(parsed_script)) {
        FURI_LOG_E(TAG, "Error parsing script");
    } else {
        jerry_value_free(jerry_run(parsed_script));
    }
    jerry_value_free(parsed_script);

    jerry_cleanup();
    return 0;
}

JsRunnerAppHandle* js_runner_alloc(JsRunner* instance) {
    JsRunnerAppHandle* handle = malloc(sizeof(JsRunnerAppHandle));

    handle->instance = instance;
    handle->thread = furi_thread_alloc_ex("js", 4 * 1024, js_app_thread_callback, handle);
    return handle;
}

void js_runner_free(JsRunnerAppHandle* handle) {
    furi_thread_free(handle->thread);
    free(handle);
}

JsRunnerError js_runner_run(
    JsRunnerAppHandle* handle,
    const char* filename,
    JsRunnerConsoleWriteCallback console_write_cb) {
    FURI_LOG_I(TAG, "Running script: %s", filename);

    JsRunner* instance = handle->instance;

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
            .script = buf,
            .script_size = file_size,
            .heap_size = 8192,
            .jrs_context = NULL,
            .status = AppStatusIdle,
            .console_callback = console_write_cb,
        };
        {
            furi_mutex_acquire(instance->apps_mutex, FuriWaitForever);
            AppDict_set_at(instance->apps, handle->thread, app);
            furi_mutex_release(instance->apps_mutex);
        }

        furi_thread_start(handle->thread);
    } while(false);
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
    return ret;
}

JsRunnerError js_runner_join(const JsRunnerAppHandle* handle) {
    furi_thread_join(handle->thread);
    return JsRunnerErrorNone;
}

static JsRunner* js_runner_create(void) {
    JsRunner* instance = malloc(sizeof(JsRunner));
    instance->event_loop = furi_event_loop_alloc();
    instance->apps_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    AppDict_init(instance->apps);
    // instance->message_queue = furi_message_queue_alloc(uint32_t msg_count, uint32_t msg_size)
    // furi_event_loop_subscribe_message_queue(
    //     instance->event_loop,
    //     instance->message_queue,
    //     FuriEventLoopEventIn,
    //     message_queue_callback,
    //     instance);
    furi_record_create(RECORD_JS_RUNNER, instance);
    return instance;
}

int32_t js_runner_srv(void* p) {
    UNUSED(p);

    FURI_LOG_I(TAG, "Service starting...");

    JsRunner* instance = js_runner_create();
    furi_event_loop_run(instance->event_loop);

    return 0;
}
