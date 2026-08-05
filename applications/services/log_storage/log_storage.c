#include "log_storage.h"

#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>

#include <storage/storage.h>
#include <string.h>

#define TAG "LogStorage"

#define LOG_STORAGE_LOG_BUFFER_SIZE (8u * 1024u)
#define LOG_STORAGE_LOCK_TIMEOUT_MS 1500u

struct LogStorage {
    FuriEventLoop* event_loop;
    FuriStreamBuffer* serial_buf;
    FuriMutex* lock;

    uint8_t log_buffer[LOG_STORAGE_LOG_BUFFER_SIZE];
    size_t head_idx;
    size_t bytes_count;
    _Atomic uint32_t overrun_count;
};

static void log_storage_on_log(const uint8_t* data, size_t size, void* context) {
    LogStorage* instance = context;

    if(size >= LOG_STORAGE_LOG_BUFFER_SIZE) return;
    if(furi_mutex_acquire(instance->lock, furi_ms_to_ticks(LOG_STORAGE_LOCK_TIMEOUT_MS)) !=
       FuriStatusOk) {
        return;
    }

    size_t space_till_wrap = MIN(LOG_STORAGE_LOG_BUFFER_SIZE - instance->head_idx, size);
    memcpy(&instance->log_buffer[instance->head_idx], data, space_till_wrap);
    if(size > space_till_wrap) {
        memcpy(instance->log_buffer, &data[space_till_wrap], size - space_till_wrap);
    }

    instance->head_idx = (instance->head_idx + size) % LOG_STORAGE_LOG_BUFFER_SIZE;
    instance->bytes_count = MIN(instance->bytes_count + size, LOG_STORAGE_LOG_BUFFER_SIZE);

    furi_check(furi_mutex_release(instance->lock) == FuriStatusOk);
}

static void log_storage_stream_buffer_callback(FuriEventLoopObject* obj, void* context) {
    furi_assert(context);
    LogStorage* instance = context;

    furi_assert(obj == instance->serial_buf);

    uint8_t tmp[512];

    for(;;) {
        if(instance->overrun_count != 0) {
            FURI_LOG_W(TAG, "Si917 log overrun %lu times", instance->overrun_count);
            instance->overrun_count = 0;
        }

        const size_t rx_size =
            furi_stream_buffer_receive(instance->serial_buf, &tmp, sizeof(tmp), 0);

        if(rx_size == 0) {
            break;
        }

        furi_log_tx(tmp, rx_size);
    }
}

static void log_storage_serial_rx_callback(
    FuriHalSerialHandle* handle,
    FuriHalSerialRxEvent event,
    void* context) {
    furi_assert(handle);
    furi_assert(context);

    LogStorage* instance = context;

    if(event == FuriHalSerialRxEventData) {
        while(furi_hal_serial_rx_available(handle)) {
            const uint8_t c = furi_hal_serial_rx(handle);
            if(furi_stream_buffer_send(instance->serial_buf, &c, 1, 0) != 1) {
                ++instance->overrun_count;
            }
        }
    }
}

bool log_storage_dump(LogStorage* instance, const char* path) {
    furi_check(instance);

    if(furi_mutex_acquire(instance->lock, furi_ms_to_ticks(LOG_STORAGE_LOCK_TIMEOUT_MS)) !=
       FuriStatusOk) {
        return false;
    }

    /* create log snapshot */
    size_t bytes_count = instance->bytes_count;
    size_t start_idx = (instance->head_idx + LOG_STORAGE_LOG_BUFFER_SIZE - bytes_count) %
                       LOG_STORAGE_LOG_BUFFER_SIZE;
    uint8_t* log_snapshot = malloc(bytes_count > 0 ? bytes_count : 1);

    size_t space_till_wrap = MIN(LOG_STORAGE_LOG_BUFFER_SIZE - start_idx, bytes_count);
    memcpy(log_snapshot, &instance->log_buffer[start_idx], space_till_wrap);
    if(bytes_count > space_till_wrap) {
        memcpy(
            &log_snapshot[space_till_wrap], instance->log_buffer, bytes_count - space_till_wrap);
    }

    furi_check(furi_mutex_release(instance->lock) == FuriStatusOk);

    /* trim leading (only in case of wrapping) */
    size_t begin = 0;
    if(bytes_count == LOG_STORAGE_LOG_BUFFER_SIZE) {
        const uint8_t* new_line = memchr(log_snapshot, '\n', bytes_count);
        begin = new_line ? (size_t)(new_line - log_snapshot) + 1 : bytes_count;
    }

    /* trim trailing (log entries are printed in multiple TX calls) */
    const uint8_t* new_line = memrchr(log_snapshot + begin, '\n', bytes_count - begin);
    size_t end = new_line ? (size_t)(new_line - log_snapshot) + 1 : begin;

    /* save log dump to file */
    size_t length = end - begin;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool is_successful = storage_simply_write_entire_file(
        storage,
        path ?: LOG_STORAGE_DUMP_DEFAULT_FILE_PATH,
        length ? log_snapshot + begin : log_snapshot,
        length);
    if(length == 0) is_successful = true;
    furi_record_close(RECORD_STORAGE);

    free(log_snapshot);
    return is_successful;
}

int32_t log_storage_srv(void* arg) {
    UNUSED(arg);

    LogStorage* instance = malloc(sizeof(*instance));

    instance->event_loop = furi_event_loop_alloc();
    instance->serial_buf = furi_stream_buffer_alloc(512, 1);
    instance->lock = furi_mutex_alloc(FuriMutexTypeNormal);
    instance->head_idx = 0;
    instance->bytes_count = 0;

    furi_record_create(RECORD_LOG_STORAGE, instance);

    furi_check(furi_log_add_handler((FuriLogHandler){
        .callback = log_storage_on_log,
        .context = instance,
    }));

    furi_event_loop_subscribe_stream_buffer(
        instance->event_loop,
        instance->serial_buf,
        FuriEventLoopEventIn,
        log_storage_stream_buffer_callback,
        instance);

    FuriHalSerialHandle* serial = furi_hal_serial_control_acquire(FuriHalSerialIdUsart2);
    furi_hal_serial_init(serial, 230400);
    furi_hal_serial_set_rx_callback(serial, log_storage_serial_rx_callback, instance);
    furi_hal_serial_async_rx_start(serial, false);

    furi_event_loop_run(instance->event_loop);

    return 0;
}
