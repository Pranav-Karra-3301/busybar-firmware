#include <furi.h>
#include <intercom/intercom.h>

#include "ble_common.h"
#include "ble_worker.h"

#define TAG "BLE"

///TODO: this should be common for both chips
typedef enum {
    BleEventTypeFrameReceived
} BleEventType;

typedef struct {
    FuriEventLoop* event_loop;
    Intercom* intercom;
    BleIntercomFrame frame;
} Ble;

void ble_custom_event_callback(uint32_t events, void* context) {
    UNUSED(events);
    Ble* instance = context;
    UNUSED(instance);

    if(events != BleEventTypeFrameReceived) {
        BLE_LOG_W("Skip event: %ld", events);
        return;
    }

    if(instance->frame.type == BleRequestTypeEnable) {
        BLE_LOG_I("Enable");
        ble_worker_start();
    } else if(instance->frame.type == BleRequestTypeDisable) {
        BLE_LOG_I("Disable");
        ble_worker_stop();
    } else if(instance->frame.type == BleRequestTypeWrite) {
        const BleIntercomFrame* frame = &instance->frame;
        BLE_LOG_I("Write Char: %04X Data_size: %d", frame->char_index, frame->data_size);
    }

    instance->frame.data_size = 0;
    size_t data_size = instance->frame.data_size + sizeof(instance->frame.type) +
                       sizeof(instance->frame.data_size) + sizeof(instance->frame.char_index);
    size_t tx_size =
        intercom_tx(instance->intercom, IntercomChannelBle, &instance->frame, data_size, 100);
    furi_assert(data_size == tx_size);
}

static void ble_backend_intercom_rx_callback(const void* data, size_t data_size, void* context) {
    furi_assert(context);
    furi_assert(data_size < MAX_BLE_INTERCOM_FRAME_SIZE);

    Ble* instance = context;
    memcpy(&instance->frame, data, data_size);
    furi_event_loop_set_custom_event(instance->event_loop, BleEventTypeFrameReceived);
}

static Ble* ble_alloc() {
    ble_worker_init();

    Ble* instance = malloc(sizeof(Ble));

    instance->event_loop = furi_event_loop_alloc();
    //     instance->event_pubsub = furi_pubsub_alloc();

    furi_event_loop_set_custom_event_callback(
        instance->event_loop, ble_custom_event_callback, instance);

    instance->intercom = furi_record_open(RECORD_INTERCOM);
    intercom_set_rx_callback(
        instance->intercom, IntercomChannelBle, ble_backend_intercom_rx_callback, instance);

    return instance;
}

int32_t ble_srv(void* arg) {
    UNUSED(arg);

    Ble* instance = ble_alloc();
    furi_event_loop_run(instance->event_loop);

    return 0;
}
