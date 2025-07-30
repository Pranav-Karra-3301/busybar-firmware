#include "ble.h"
#include "ble_common.h"
#include "ble_service.h"

#include <intercom/intercom.h>
#include <api_lock.h>

#define TAG "BLE"

typedef enum {
    BleStateTypeIdle,
    BleStateTypeIniting,
    BleStateTypeReady,
} BleStateType;

typedef enum {
    BleEventTypeInit,
    BleEventTypeIncomingMessage,
    BleEventTypeFrameReceived,
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

    BleServiceObject* services[3];
    BleIntercomServiceIndex pending_service_index;
    BleStateType state;

    BleIntercomFrameGeneric frame;
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
            instance->frame.header.type = instance->current_message->type;
            instance->frame.header.data_size = 0;

            size_t data_size = instance->frame.header.data_size + sizeof(BleIntercomFrameHeader);
            size_t tx_size = intercom_tx(
                instance->intercom, IntercomChannelBle, &instance->frame, data_size, 100);

            furi_assert(data_size == tx_size);
        } else
            BLE_LOG_W("Wrong message type");

    } else if(events == BleEventTypeFrameReceived) {
        BLE_LOG_I("Frame received");
        if(instance->frame.header.type == BleRequestTypeWrite) {
            BleServiceObject* service = instance->services[instance->pending_service_index];
            if(service->desc->on_response) {
                service->desc->on_response(service, &instance->frame);
            }
        } else if((instance->frame.header.type == BleRequestTypeInit)) {
            if(instance->frame.header.service_index == instance->pending_service_index) {
                BleServiceObject* service = instance->services[instance->pending_service_index];
                if(service->desc->on_response) {
                    service->desc->on_response(service, &instance->frame);
                }

                // instance->pending_service_index++;
                if(instance->pending_service_index == 3) {
                    instance->state = BleStateTypeReady;
                    BLE_LOG_W("Init done");
                }
                //update service state from ble_services
                // if(instance->state != BleStateTypeReady) {
                //     furi_event_loop_set_custom_event(instance->event_loop, BleEventTypeInit);
                // }

            } else
                BLE_LOG_W("Service index not match");
        } else {
            if(instance->current_message) {
                BleMessage* message = instance->current_message;
                message->result = (instance->frame.header.type == BleRequestTypeEnable) ||
                                  (instance->frame.header.type == BleRequestTypeDisable);

                instance->current_message = NULL;
                api_lock_unlock(message->lock);
            }
            furi_semaphore_release(instance->access_semaphore);
        }
        // furi_semaphore_release(instance->access_semaphore);
    } else if(events == BleEventTypeInit) {
        bool result = false;
        do {
            uint16_t index = instance->pending_service_index;
            BleServiceObject* service = instance->services[index];
            result = ble_service_common_init(service, &instance->frame);
            if(!result) instance->pending_service_index++;
        } while(!result);
    }
}

static void ble_backend_intercom_rx_callback(const void* data, size_t data_size, void* context) {
    furi_assert(context);
    furi_assert(data_size < MAX_BLE_INTERCOM_FRAME_SIZE);

    Ble* instance = context;
    memcpy(&instance->frame, data, data_size);
    furi_event_loop_set_custom_event(instance->event_loop, BleEventTypeFrameReceived);
}

static void ble_event_loop_on_start(void* context) {
    BLE_LOG_W("ble_event_loop_on_start");
    Ble* ble = context;
    ble->pending_service_index = 0;
    ble->state = BleStateTypeIniting;
    furi_delay_ms(10000);
    furi_event_loop_set_custom_event(ble->event_loop, BleEventTypeInit);
}

static Ble* ble_alloc() {
    Ble* instance = malloc(sizeof(Ble));

    instance->event_loop = furi_event_loop_alloc();
    instance->access_semaphore = furi_semaphore_alloc(1, 1);

    //     instance->event_pubsub = furi_pubsub_alloc();

    furi_event_loop_set_custom_event_callback(
        instance->event_loop, ble_custom_event_callback, instance);

    furi_event_loop_pend_callback(instance->event_loop, ble_event_loop_on_start, instance);

    instance->intercom = furi_record_open(RECORD_INTERCOM);
    intercom_set_rx_callback(
        instance->intercom, IntercomChannelBle, ble_backend_intercom_rx_callback, instance);

    for(size_t i = 0; i < 3; i++) {
        instance->services[i] = ble_worker_create_service(
            &service_config[i], instance->access_semaphore, instance->intercom);
    }

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
