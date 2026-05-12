#include <furi.h>
#include <furi_hal_random.h>

#include <psa/crypto.h>

#include <mbedtls/ssl.h>
#include <mbedtls/threading.h>
#include <mbedtls/memory_buffer_alloc.h>

#define MBEDTLS_HEAP_SIZE (500000UL)

#define TAG "mbedtls"

static uint8_t mbedtls_heap[MBEDTLS_HEAP_SIZE];

static void mutex_init(FuriMutex** mutex) {
    furi_assert(mutex);
    *mutex = furi_mutex_alloc(FuriMutexTypeNormal);
}

static void mutex_free(FuriMutex** mutex) {
    furi_assert(mutex);
    furi_mutex_free(*mutex);
}

static int mutex_lock(FuriMutex** mutex) {
    furi_assert(mutex);
    return furi_mutex_acquire(*mutex, FuriWaitForever) != FuriStatusOk;
}

static int mutex_unlock(FuriMutex** mutex) {
    furi_assert(mutex);
    return furi_mutex_release(*mutex) != FuriStatusOk;
}

psa_status_t mbedtls_psa_external_get_random(
    mbedtls_psa_external_random_context_t* context,
    uint8_t* output,
    size_t output_size,
    size_t* output_length) {
    UNUSED(context);

    furi_hal_random_fill_buf(output, output_size);
    *output_length = output_size;

    return PSA_SUCCESS;
}

void mbedtls_glue_init(void) {
    // WARNING: The function call order below is important,
    //          change only if you know what you are doing!
    mbedtls_threading_set_alt(mutex_init, mutex_free, mutex_lock, mutex_unlock);
    mbedtls_memory_buffer_alloc_init(mbedtls_heap, sizeof(mbedtls_heap));
}

void _exit(int i) {
    UNUSED(i);
    furi_crash("_exit");
}
