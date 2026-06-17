#include <furi/furi.h>
#include <jerryscript-port.h>

#define THREAD_STORAGE_INDEX 1
#define HEAP_SIZE 8192

size_t jerry_port_context_alloc(size_t context_size) {
    void* tlp = furi_thread_local_storage_pointer_get(NULL, THREAD_STORAGE_INDEX);
    furi_check(tlp == NULL);
    size_t alloc_size = context_size + HEAP_SIZE;
    tlp = malloc(alloc_size);
    furi_thread_local_storage_pointer_set(NULL, THREAD_STORAGE_INDEX, tlp);
    return alloc_size;
}

struct jerry_context_t *jerry_port_context_get(void) {
    return furi_thread_local_storage_pointer_get(NULL, THREAD_STORAGE_INDEX);
}

void jerry_port_context_free(void) {
    void* tlp = furi_thread_local_storage_pointer_get(NULL, THREAD_STORAGE_INDEX);
    free(tlp);
    furi_thread_local_storage_pointer_set(NULL, THREAD_STORAGE_INDEX, NULL);
}
