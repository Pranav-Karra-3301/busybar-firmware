/**
 * @file js_runner_i.h
 *
 * @brief Javascript app runner - private declarations
 */
#pragma once
#include "js_runner.h"
#include "js_util.h"
#include <furi/furi.h>
#include <storage/storage.h>
#include <path.h>
#include <fetch/fetch.h>

#include <m-dict.h>
#include <m-array.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
#include <jerryscript.h>
#pragma GCC diagnostic pop

#define MIN_INTERVAL_DELAY_MS 10.0f
#define MAX_FETCH_MESSAGES    32

#define PTR_HASH(p) ((size_t)(p))

typedef enum ChildStatus {
    ChildStatusNotYet,
    ChildStatusRunning,
    ChildStatusDone
} ChildStatus;

#define JS_RUNNER_MAX_SCRIPT_SIZE (250 * 1024)

typedef struct IntervalContext {
    bool once;
    FuriEventLoopTimer* timer;
    jerry_value_t callback;
} IntervalContext;

M_DICT_DEF2(IntervalDict, uint32_t, M_DEFAULT_OPLIST, IntervalContext, M_POD_OPLIST);
ARRAY_DEF(ByteArray, uint8_t);

typedef struct JsRunnerApp {
    size_t heap_size;
    void* jrs_context;
    FuriEventLoop* event_loop;
    JsRunnerConsoleOutCallback console_callback;
    void* console_callback_context;
    FuriString* root_path;

    IntervalDict_t intervals;
    uint32_t last_interval_id;

    uint32_t num_fetch_threads;
    FuriMessageQueue* fetch_event_queue;
} JsRunnerApp;

M_DICT_DEF2(
    AppDict,
    FuriThread*,
    M_OPEXTEND(M_PTR_OPLIST, HASH(PTR_HASH)),
    JsRunnerApp*,
    M_PTR_OPLIST);

typedef struct JsRunner {
    FuriEventLoop* event_loop;
    FuriMessageQueue* message_queue;

    FuriMutex* apps_mutex;
    AppDict_t apps;
} JsRunner;

#define WITH_JS_RUNNER_APP(APP, BLOCK)                                     \
    do {                                                                   \
        JsRunner* __instance = furi_record_open(RECORD_JS_RUNNER);         \
        furi_mutex_acquire(__instance->apps_mutex, FuriWaitForever);       \
        FuriThread* current_thread = furi_thread_get_current();            \
        JsRunnerApp* APP = *AppDict_get(__instance->apps, current_thread); \
        if(APP) {                                                          \
            BLOCK                                                          \
        } else {                                                           \
            FURI_LOG_E(TAG, "No JS app handle for current thread");        \
            furi_crash();                                                  \
        }                                                                  \
        furi_mutex_release(__instance->apps_mutex);                        \
        furi_record_close(RECORD_JS_RUNNER);                               \
    } while(false)

#define JS_ARG(n) (args_count > (n) ? args[(n)] : jerry_undefined())

void js_check_and_free(jerry_value_t val);
void js_runner_check_event_loop(JsRunnerApp* app);
void js_run_jobs(void);

/** @brief Allocate Jerryscript context for current thread. This function is used by jerryscript glue. */
size_t js_runner_context_alloc(size_t context_size);

/** @brief Free Jerryscript context for current thread. This function is used by jerryscript glue. */
void js_runner_context_free(void);

/** @brief Get Jerryscript context for current thread. This function is used by jerryscript glue. */
void* js_runner_context_get(void);

/** @brief Get root path of the current JS app (folder containg entry point).
 * This function is used by jerryscript glue. */
void js_runner_get_root_path(FuriString* path);
