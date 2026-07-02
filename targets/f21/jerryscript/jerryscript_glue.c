#include <furi/furi.h>
#include <time/time.h>
#include <js_runner/js_runner.h>

#include <utz/utz.h>
#include <jerryscript.h>

#define TAG "JSGlue"

size_t jerry_port_context_alloc(size_t context_size) {
    JsRunner* js_runner = furi_record_open(RECORD_JS_RUNNER);
    size_t result = js_runner_context_alloc(js_runner, context_size);
    furi_record_close(RECORD_JS_RUNNER);
    return result;
}

struct jerry_context_t* jerry_port_context_get(void) {
    JsRunner* js_runner = furi_record_open(RECORD_JS_RUNNER);
    void* result = js_runner_context_get(js_runner);
    furi_record_close(RECORD_JS_RUNNER);
    return result;
}

void jerry_port_context_free(void) {
    JsRunner* js_runner = furi_record_open(RECORD_JS_RUNNER);
    js_runner_context_free(js_runner);
    furi_record_close(RECORD_JS_RUNNER);
}

void jerry_port_init(void) {
}

void jerry_port_fatal(jerry_fatal_code_t code) {
    FURI_LOG_E(TAG, "jerryscript fatal error %d", code);
    furi_crash(code);
}

double jerry_port_current_time(void) {
    return (double)time_get_timestamp_ms();
}

int32_t jerry_port_local_tza(double unix_ms) {
    Time* time = furi_record_open(RECORD_TIME);
    TimeSettings settings;
    time_get_settings(time, &settings);
    furi_record_close(RECORD_TIME);
    DateTimeMs dt = datetime_timestamp_ms_to_datetime((time_t)unix_ms);
    utz_offset_t offset;
    utz_get_current_offset(&settings.timezone, &dt.dt, &offset);
    return (offset.hours * 60 + offset.minutes) * 60 * 1000;
}
