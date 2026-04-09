#include "ble_streaming.h"
#include "state_publisher/state_publisher.h"

#define TAG "BleStream"

#define MAX_TX_CHUNK_SIZE (237)

#define BLE_STREAM_WAIT_TX_TIMEOUT_MS        (250)
#define BLE_STREAM_FRAME_PERIOD_MS           (1000)
#define BLE_STREAM_RATE_LIMITER_PERIOD_MS    (1000)
#define BLE_STREAM_RATE_LIMITER_MAX_PACK_CNT (1)

#define BLE_STREAM_STATE_PUBLISHER_INVALID_HANDLE (0xDEADBEEF)

struct BleStreaming {
    bool run;
    FuriMutex* lock;
    FuriSemaphore* wait_tx;
    StatePublisherTransportHandle handle;
    Ble* ble;
};

static void ble_streaming_start(BleStreaming* instance);
static void ble_streaming_stop(BleStreaming* instance);

static void ble_uart_tx_done_callback(void* context) {
    BleStreaming* instance = context;
    furi_semaphore_release(instance->wait_tx);
}

static void
    ble_streaming_send_data(BleStreaming* instance, const uint8_t* data, size_t data_size) {
    furi_mutex_acquire(instance->lock, FuriWaitForever);

    size_t index = 0;
    while(data_size && instance->run) {
        size_t send_size = data_size > MAX_TX_CHUNK_SIZE ? MAX_TX_CHUNK_SIZE : data_size;

        ble_uart_tx_data(instance->ble, BleUartChannelHM10, &data[index], send_size);

        FuriStatus status =
            furi_semaphore_acquire(instance->wait_tx, BLE_STREAM_WAIT_TX_TIMEOUT_MS);
        if(status != FuriStatusOk) break;

        data_size -= send_size;
        index += send_size;
    }
    furi_mutex_release(instance->lock);
}

static void ble_stream_state_publisher_callback(const SharedByteArray_t data, void* context) {
    BleStreaming* instance = context;

    SharedByteArray_t my_data;
    SharedByteArray_init_set(my_data, data);

    const ByteArray_t* array = SharedByteArray_cref(my_data);
    const uint8_t* payload = ByteArray_cget(*array, 0);
    const size_t size = ByteArray_size(*array);

    ble_streaming_send_data(instance, payload, size);
    SharedByteArray_clear(my_data);
}

BleStreaming* ble_streaming_alloc(Ble* ble) {
    BleStreaming* instance = malloc(sizeof(BleStreaming));
    instance->lock = furi_mutex_alloc(FuriMutexTypeNormal);
    instance->wait_tx = furi_semaphore_alloc(1, 0);
    instance->ble = ble;
    instance->handle = BLE_STREAM_STATE_PUBLISHER_INVALID_HANDLE;
    instance->run = false;
    return instance;
}

void ble_streaming_free(BleStreaming* instance) {
    furi_assert(instance);

    ble_streaming_stop(instance);
    furi_semaphore_free(instance->wait_tx);
    furi_mutex_free(instance->lock);
    free(instance);
}

static inline void ble_stream_state_publisher_subscribe(BleStreaming* instance) {
    StatePublisher* state_publisher = furi_record_open(RECORD_STATE_PUBLISHER);

    RateLimiterLimit limit = {
        .period_ms = BLE_STREAM_RATE_LIMITER_PERIOD_MS,
        .max_packet_count = BLE_STREAM_RATE_LIMITER_MAX_PACK_CNT,
    };

    instance->handle = state_publisher_add_transport(
        state_publisher,
        StatePublisherTransportClassBLE,
        BLE_STREAM_FRAME_PERIOD_MS,
        limit,
        ble_stream_state_publisher_callback,
        instance);

    furi_record_close(RECORD_STATE_PUBLISHER);
}

static inline void ble_stream_state_publisher_unsubscribe(BleStreaming* instance) {
    StatePublisher* state_publisher = furi_record_open(RECORD_STATE_PUBLISHER);
    state_publisher_del_transport(state_publisher, instance->handle);
    furi_record_close(RECORD_STATE_PUBLISHER);
    instance->handle = BLE_STREAM_STATE_PUBLISHER_INVALID_HANDLE;
}

static void ble_streaming_start(BleStreaming* instance) {
    furi_assert(instance);
    furi_mutex_acquire(instance->lock, FuriWaitForever);

    if(!instance->run) {
        FURI_LOG_D(TAG, "Stream start");

        instance->run = true;
        ble_uart_set_tx_done_callback(
            instance->ble, BleUartChannelHM10, ble_uart_tx_done_callback, instance);
        ble_stream_state_publisher_subscribe(instance);
    }
    furi_mutex_release(instance->lock);
}

static void ble_streaming_stop(BleStreaming* instance) {
    furi_assert(instance);
    furi_mutex_acquire(instance->lock, FuriWaitForever);

    if(instance->run) {
        instance->run = false;
        ble_stream_state_publisher_unsubscribe(instance);

        ble_uart_set_tx_done_callback(instance->ble, BleUartChannelHM10, NULL, NULL);

        FURI_LOG_D(TAG, "Stream stopped");
    }
    furi_mutex_release(instance->lock);
}

void ble_streaming_update(BleStreaming* instance, const BleServiceStatus status) {
    furi_assert(instance);
    furi_assert(status < BleServiceStatusCount);

    if(status == BleServiceStatusConnected)
        ble_streaming_start(instance);
    else
        ble_streaming_stop(instance);
}
