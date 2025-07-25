#include <furi.h>
#include <intercom/intercom.h>

#include "ble_worker.h"

#define TAG "BLE"

///TODO: this should be common for both chips
typedef enum {
    BleEventTypeEnable,
    BleEventTypeDisable,

} BleEventType;

typedef struct {
    FuriEventLoop* event_loop;
    Intercom* intercom;
} Ble;

void ble_custom_event_callback(uint32_t events, void* context) {
    UNUSED(events);
    Ble* instance = context;
    UNUSED(instance);

    ble_worker_start();
}

static void ble_backend_intercom_rx_callback(const void* data, size_t data_size, void* context) {
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
        instance->intercom, IntercomChannelWifi, ble_backend_intercom_rx_callback, instance);

    //     furi_record_create(RECORD_WIFI, instance->event_pubsub);

    furi_event_loop_set_custom_event(instance->event_loop, 1);

    return instance;
}

int32_t ble_srv(void* arg) {
    UNUSED(arg);

    Ble* instance = ble_alloc();
    furi_event_loop_run(instance->event_loop);

    return 0;
}
