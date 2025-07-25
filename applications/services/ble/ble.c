#include "ble.h"
#include "ble_common.h"

#include <intercom/intercom.h>
#include <api_lock.h>

#define TAG "BLE"

typedef enum {
    BleEventTypeIncomingMessage,
    BleEventTypeFrameReceived
} BleEventType;

typedef struct {
    BleRequestType type; ///TODO: get rid if this
    FuriApiLock lock;
    bool result; ///TODO: replace with some more extended status
} BleMessage;

struct Ble {
    FuriSemaphore* access_semaphore;
    FuriEventLoop* event_loop;
    Intercom* intercom;
    BleIntercomFrame frame;
    BleMessage* current_message;
};

void ble_custom_event_callback(uint32_t events, void* context) {
    UNUSED(events);
    Ble* instance = context;
    UNUSED(instance);

    if(events == BleEventTypeIncomingMessage) {
        BLE_LOG_I("Incomming message");
        if(instance->current_message->type == BleRequestTypeEnable ||
           instance->current_message->type == BleRequestTypeDisable) {
            instance->frame.type = instance->current_message->type;
            instance->frame.data_size = 0;

            size_t data_size = instance->frame.data_size + sizeof(instance->frame.type) +
                               sizeof(instance->frame.data_size) +
                               sizeof(instance->frame.char_index);
            size_t tx_size = intercom_tx(
                instance->intercom, IntercomChannelBle, &instance->frame, data_size, 100);

            furi_assert(data_size == tx_size);
        } else
            BLE_LOG_W("Wrong message type");
    } else if(events == BleEventTypeFrameReceived) {
        BLE_LOG_I("Frame received");
        BleMessage* message = instance->current_message;
        message->result = (instance->frame.type == BleRequestTypeEnable) ||
                          (instance->frame.type == BleRequestTypeDisable);
        api_lock_unlock(message->lock);
        furi_semaphore_release(instance->access_semaphore);
    }
}

static void ble_backend_intercom_rx_callback(const void* data, size_t data_size, void* context) {
    furi_assert(context);
    furi_assert(data_size < MAX_BLE_INTERCOM_FRAME_SIZE);

    Ble* instance = context;
    memcpy(&instance->frame, data, data_size);
    furi_event_loop_set_custom_event(instance->event_loop, BleEventTypeFrameReceived);
}

static Ble* ble_alloc() {
    Ble* instance = malloc(sizeof(Ble));

    instance->event_loop = furi_event_loop_alloc();
    instance->access_semaphore = furi_semaphore_alloc(1, 1);

    //     instance->event_pubsub = furi_pubsub_alloc();

    furi_event_loop_set_custom_event_callback(
        instance->event_loop, ble_custom_event_callback, instance);

    instance->intercom = furi_record_open(RECORD_INTERCOM);
    intercom_set_rx_callback(
        instance->intercom, IntercomChannelBle, ble_backend_intercom_rx_callback, instance);

    furi_record_create(RECORD_BLE, instance);
    return instance;
}

int32_t ble_srv(void* arg) {
    UNUSED(arg);

    Ble* instance = ble_alloc();
    furi_event_loop_run(instance->event_loop);

    return 0;
}

static void ble_send_message(Ble* instance, BleMessage* message) {
    message->lock = api_lock_alloc_locked();

    furi_check(
        furi_semaphore_acquire(instance->access_semaphore, FuriWaitForever) == FuriStatusOk);

    instance->current_message = message;
    furi_event_loop_set_custom_event(instance->event_loop, BleEventTypeIncomingMessage);

    api_lock_wait_unlock_and_free(message->lock);
}

bool ble_start(Ble* ble) {
    BleMessage msg;
    msg.type = BleRequestTypeEnable;
    ble_send_message(ble, &msg);
    return msg.result;
}

bool ble_stop(Ble* ble) {
    BleMessage msg;
    msg.type = BleRequestTypeDisable;
    ble_send_message(ble, &msg);
    return msg.result;
}
