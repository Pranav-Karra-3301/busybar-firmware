#include "ble_streaming.h"
#include "state_publisher/state_publisher.h"

#define TAG "BleStream"

#define MAX_TX_CHUNK_SIZE (237)

#define RAW_BUFFER_SIZE (6400U)

typedef enum {
    BleStreamingEventFramePending,
} BleStreamingEvent;

typedef struct {
    FuriEventLoop* event_loop;
    FuriEventLoopTimer* timer;
    FuriThread* thread;
    FuriSemaphore* wait_tx;
    FuriMutex* mutex;

    FuriStreamBuffer* stream_buffer;
    StatePublisherTransportHandle handle;
    Ble* ble;
    // Gui* gui;

    // uint8_t* compressed_buffer;
    // uint8_t* raw_buffer;
    // GuiDisplayId display_id;

    // uint8_t* data_buffer;
    // size_t data_size;
    // size_t max_size;

    // const SharedByteArray_t data;

    // size_t frame_size;

} BleStreaming;

static FuriMutex* ble_streaming_init_mutex;
static BleStreaming* ble_streaming_instance;

static void ble_uart_rx_callback(size_t data_size, void* data, void* context) {
    UNUSED(data);
    UNUSED(data_size);
    UNUSED(context);
    FURI_LOG_D(TAG, "ble_uart_rx_callback");
    // if(data_size != sizeof(uint8_t)) {
    //     FURI_LOG_W(TAG, "Not uint8_t, skip!");
    //     return;
    // }

    // const BleStreamingMode event = *(uint8_t*)data;
    // BleStreaming* instance = context;
    // furi_event_loop_set_custom_event(instance->event_loop, event);
}

static bool
    ble_streaming_send_data(BleStreaming* instance, const uint8_t* data, size_t data_size) {
    size_t index = 0;

    bool result = true;
    while(data_size) {
        size_t send_size = data_size > MAX_TX_CHUNK_SIZE ? MAX_TX_CHUNK_SIZE : data_size;

        ble_uart_tx_data(instance->ble, BleUartChannelHM10, &data[index], send_size);
        if(furi_semaphore_acquire(instance->wait_tx, 500) != FuriStatusOk) {
            FURI_LOG_W(TAG, "Wait_tx fail");
            result = false;
            break;
        }

        data_size -= send_size;
        index += send_size;
    }
    return result;
}

static void state_publisher_callback(const SharedByteArray_t data, void* context) {
    BleStreaming* instance = context;

    SharedByteArray_t my_data;
    SharedByteArray_init_set(my_data, data);

    const ByteArray_t* array = SharedByteArray_cref(my_data);
    const uint8_t* payload = ByteArray_cget(*array, 0);
    const size_t size = ByteArray_size(*array);

    FURI_LOG_I(TAG, "Data size: %d", size);
    if(ble_streaming_send_data(instance, payload, size)) {
        FURI_LOG_W(TAG, "Send done");
    } else
        FURI_LOG_W(TAG, "Send fail");

    // if(instance->max_size < data_size) {
    //     instance->data_buffer = realloc(instance->data_buffer, data_size);
    //     FURI_LOG_W(TAG, "buffer realloced");
    //     instance->max_size = data_size;
    // }

    // memcpy(instance->data_buffer, data->inner, data_size);
    // instance->data_size = data_size;

    // furi_event_loop_set_custom_event(instance->event_loop, BleStreamingEventFramePending);
    SharedByteArray_clear(my_data);
}

static void ble_uart_tx_done_callback(void* context) {
    BleStreaming* instance = context;
    furi_semaphore_release(instance->wait_tx);
}

static void ble_streaming_event_loop_callback(uint32_t events, void* context) {
    BleStreaming* instance = context;
    UNUSED(instance);
    if(events == BleStreamingEventFramePending) {
        // FURI_LOG_I(TAG, "Data size: %d", instance->data_size);
        // if(ble_streaming_send_data(instance, instance->data_buffer, instance->data_size)) {
        //     FURI_LOG_W(TAG, "Send done");
        // } else
        //     FURI_LOG_W(TAG, "Send fail");
    }
}

static int32_t ble_streaming_thread(void* context) {
    BleStreaming* instance = context;

    instance->event_loop = furi_event_loop_alloc();
    furi_event_loop_set_custom_event_callback(
        instance->event_loop, ble_streaming_event_loop_callback, instance);

    FURI_LOG_W(TAG, "Start event loop");
    furi_event_loop_run(instance->event_loop);
    FURI_LOG_W(TAG, "Stop event loop");

    furi_event_loop_free(instance->event_loop);
    return 0;
}

static BleStreaming* ble_streaming_alloc(Ble* ble) {
    BleStreaming* instance = malloc(sizeof(BleStreaming));
    instance->wait_tx = furi_semaphore_alloc(1, 0);
    instance->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    instance->ble = ble;

    ble_uart_set_rx_callback(ble, BleUartChannelHM10, ble_uart_rx_callback, instance);
    ble_uart_set_tx_done_callback(ble, BleUartChannelHM10, ble_uart_tx_done_callback, instance);

    instance->thread = furi_thread_alloc_ex(TAG, 1024 * 2, ble_streaming_thread, instance);

    StatePublisher* state_publisher = furi_record_open(RECORD_STATE_PUBLISHER);
    RateLimiterLimit limit = {.period_ms = 1000, .max_packet_count = 1};

    instance->handle = state_publisher_add_transport(
        state_publisher,
        StatePublisherTransportClassBLE,
        1000,
        limit,
        state_publisher_callback,
        instance);
    furi_record_close(RECORD_STATE_PUBLISHER);
    return instance;
}

static void ble_streaming_free(BleStreaming* instance) {
    // free(instance->data_buffer);
    furi_thread_free(instance->thread);
    furi_semaphore_free(instance->wait_tx);
    furi_mutex_free(instance->mutex);
    free(instance);
}

void ble_streaming_init() {
    furi_assert(ble_streaming_init_mutex == NULL);
    ble_streaming_init_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
}

void ble_streaming_start(Ble* ble) {
    if(furi_mutex_acquire(ble_streaming_init_mutex, 100) == FuriStatusOk) {
        if(ble_streaming_instance == NULL) {
            FURI_LOG_W(TAG, "Start ble stream");

            ble_streaming_instance = ble_streaming_alloc(ble);
            furi_thread_start(ble_streaming_instance->thread);
        }
        furi_mutex_release(ble_streaming_init_mutex);
    }
}

void ble_streaming_stop() {
    if(furi_mutex_acquire(ble_streaming_init_mutex, 100) == FuriStatusOk) {
        if(ble_streaming_instance != NULL) {
            FURI_LOG_W(TAG, "Stop ble stream");

            StatePublisher* state_publisher = furi_record_open(RECORD_STATE_PUBLISHER);
            state_publisher_del_transport(state_publisher, ble_streaming_instance->handle);
            furi_record_close(RECORD_STATE_PUBLISHER);

            furi_event_loop_stop(ble_streaming_instance->event_loop);
            furi_thread_join(ble_streaming_instance->thread);
            FURI_LOG_W(TAG, "Stopped");
            ble_streaming_free(ble_streaming_instance);
            ble_streaming_instance = NULL;
        }
        furi_mutex_release(ble_streaming_init_mutex);
    }
}
