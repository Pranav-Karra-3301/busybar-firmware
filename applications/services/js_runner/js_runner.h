#pragma once
#include <stddef.h>
#include <furi/core/string.h>

#define RECORD_JS_RUNNER "js_runner"

typedef struct JsRunner JsRunner;

typedef enum JsRunnerError {
    JsRunnerErrorNone = 0,
    JsRunnerErrorCannotOpenFile,
    JsRunnerParseException,
} JsRunnerError;

typedef enum JsRunnerConsoleSeverity {
    JsRunnerConsoleSeverityLog,
    JsRunnerConsoleSeverityInfo,
    JsRunnerConsoleSeverityError,
} JsRunnerConsoleSeverity;

typedef void (*JsRunnerConsoleWriteCallback)(
    JsRunnerConsoleSeverity severity,
    const char* buf,
    size_t size,
    void* context);

size_t js_runner_context_alloc(JsRunner* instance, size_t context_size);

void js_runner_context_free(JsRunner* instance);

void* js_runner_context_get(JsRunner* instance);

void js_runner_get_root_path(JsRunner* instance, FuriString* path);

JsRunnerError js_runner_run(
    JsRunner* instance,
    const char* path,
    size_t heap_size,
    JsRunnerConsoleWriteCallback console_write_cb,
    void* console_write_context);
