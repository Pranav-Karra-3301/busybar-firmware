#include <furi/furi.h>
#include <time/time.h>
#include <thread_local_config.h>

#include <utz/utz.h>
#include <jerryscript.h>

#define TAG       "JSGlue"
#define HEAP_SIZE 8192

size_t jerry_port_context_alloc(size_t context_size) {
    void* tlp =
        furi_thread_local_storage_pointer_get(NULL, ThreadLocalStoragePointerIdJerryscript);
    furi_check(tlp == NULL);
    size_t alloc_size = context_size + HEAP_SIZE;
    tlp = malloc(alloc_size);
    furi_thread_local_storage_pointer_set(NULL, ThreadLocalStoragePointerIdJerryscript, tlp);
    return alloc_size;
}

struct jerry_context_t* jerry_port_context_get(void) {
    return furi_thread_local_storage_pointer_get(NULL, ThreadLocalStoragePointerIdJerryscript);
}

void jerry_port_context_free(void) {
    void* tlp =
        furi_thread_local_storage_pointer_get(NULL, ThreadLocalStoragePointerIdJerryscript);
    free(tlp);
    furi_thread_local_storage_pointer_set(NULL, ThreadLocalStoragePointerIdJerryscript, NULL);
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
