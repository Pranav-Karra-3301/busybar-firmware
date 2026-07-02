#pragma once
#include <stddef.h>

#define RECORD_JS_RUNNER "js_runner"

typedef struct JsRunner JsRunner;

typedef struct JsRunnerAppHandle JsRunnerAppHandle;

typedef enum JsRunnerError {
    JsRunnerErrorNone = 0,
    JsRunnerErrorCannotOpenFile,
} JsRunnerError;

typedef void (*JsRunnerConsoleWriteCallback)(const char* buf, size_t size);

size_t js_runner_context_alloc(JsRunner* instance, size_t context_size);

void js_runner_context_free(JsRunner* instance);

void* js_runner_context_get(JsRunner* instance);

JsRunnerAppHandle* js_runner_alloc(JsRunner* instance);

void js_runner_free(JsRunnerAppHandle* handle);

JsRunnerError js_runner_run(
    JsRunnerAppHandle* handle,
    const char* filename,
    JsRunnerConsoleWriteCallback console_write_cb);

JsRunnerError js_runner_join(const JsRunnerAppHandle* handle);
