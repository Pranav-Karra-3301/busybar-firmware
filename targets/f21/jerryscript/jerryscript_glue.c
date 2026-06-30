#include <furi/furi.h>
#include <jerryscript.h>
#include <thread_local_config.h>

#define HEAP_SIZE 8192

size_t jerry_port_context_alloc(size_t context_size) {
    void* tlp = furi_thread_local_storage_pointer_get(NULL, ThreadLocalStoragePointerIdJerryscript);
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
    void* tlp = furi_thread_local_storage_pointer_get(NULL, ThreadLocalStoragePointerIdJerryscript);
    free(tlp);
    furi_thread_local_storage_pointer_set(NULL, ThreadLocalStoragePointerIdJerryscript, NULL);
}

int32_t jerry_port_local_tza(double unix_ms) {
    // TODO
    UNUSED(unix_ms);
    return 0;
}

void jerry_port_init(void) {
}

void jerry_port_fatal(jerry_fatal_code_t code) {
    furi_crash(code);
}

double jerry_port_current_time(void) {
    // TODO
    return 0.0;
}
