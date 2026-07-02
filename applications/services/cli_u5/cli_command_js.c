#include "cli_command_js.h"
#include <furi/furi.h>

#include <cli/args.h>
#include <js_runner/js_runner.h>

static void js_console_cb(const char* buf, size_t size) {
    printf("%.*s", size, buf);
}

void cli_command_js_run(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    UNUSED(context);

    if(furi_string_size(args) > 0) {
        JsRunner* runner = furi_record_open(RECORD_JS_RUNNER);
        JsRunnerAppHandle* handle = js_runner_alloc(runner);
        JsRunnerError error = js_runner_run(handle, furi_string_get_cstr(args), js_console_cb);
        if(error == JsRunnerErrorNone) {
            js_runner_join(handle);
        } else {
            printf("Error running script: %d", error);
        }
        js_runner_free(handle);
        furi_record_close(RECORD_JS_RUNNER);
    } else {
        printf("Usage: js <filename>\r\n");
    }
}
