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

/** @brief Run a JS application.
 *
 * This function blocks until the script terminates.
 *
 * @param path entry point script path.
 * @param heap_size JS heap size for the app in bytes.
 * @param console_write_cb callback function for JS console methods (console.log, console.error, console.info). Supply NULL to disable console.
 * @param console_write_context user pointer passed to console_write_cb.
 *
 * @return error code
 */
JsRunnerError js_runner_run(
    JsRunner* instance,
    const char* path,
    size_t heap_size,
    JsRunnerConsoleWriteCallback console_write_cb,
    void* console_write_context);

/** @brief Allocate Jerryscript context for current thread. This function is used by jerryscript glue. */
size_t js_runner_context_alloc(size_t context_size);

/** @brief Free Jerryscript context for current thread. This function is used by jerryscript glue. */
void js_runner_context_free(void);

/** @brief Get Jerryscript context for current thread. This function is used by jerryscript glue. */
void* js_runner_context_get(void);

/** @brief Get root path of the current JS app (folder containg entry point).
 * This function is used by jerryscript glue. */
void js_runner_get_root_path(FuriString* path);
