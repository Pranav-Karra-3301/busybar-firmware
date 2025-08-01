#include "ble.h"
#include "ble_common.h"
#include "service/ble_service.h"
#include "service/ble_service_config.h"

#include <intercom/intercom.h>
#include <api_lock.h>

#define TAG "BLE"

typedef enum {
    BleEventTypeInit,
    BleEventTypeIncomingMessage,
    BleEventTypeFrameReceived,
} BleEventType;

struct Ble {
    BleServiceState state;

    FuriMessageQueue* message_queue;

    FuriSemaphore* mailbox_lock;
    BleIntercomFrameGeneric mailbox;

    FuriEventLoop* event_loop;
    Intercom* intercom;
    //--------------------------

    FuriSemaphore* access_semaphore;

    BleServiceObject* services[3];
    BleIntercomServiceIndex pending_service_index;

    BleMessage* current_message;
};

// static bool heartbeat_processor(Ble* instance) {
//     const BleIntercomFrameHeartbeat* frame = (BleIntercomFrameHeartbeat*)&instance->mailbox;
//     if(frame->state == BleServiceStateInitialization) {
//         BleMessage msg = {
//             .type = BleCommandServiceSetState,
//             .service_index = 0,
//             .data[0] = BleServiceStateInitialization};
//         furi_assert(furi_message_queue_put(instance->message_queue, &msg, 100) == FuriStatusOk);
//         //instance->state = BleServiceStateInitialization;
//     }

//     return true;
// }

// static bool ble_base_process_frame(Ble* instance) {
//     furi_assert(instance);
//     // BLE_LOG_D("ble_base_process_frame");

//     // const BleIntercomFrameGeneric* frame = &instance->mailbox;
//     // if(frame->header.new_type == BleIntercomFrameTypeHeartbeat) {
//     //     heartbeat_processor(instance);
//     // }

//     // return true;
// }

static void ble_event_loop_msg_queue_handler(FuriEventLoopObject* object, void* context) {
    furi_assert(context);
    Ble* ble = context;
    furi_assert(object == ble->message_queue);

    BleMessage msg;
    if(furi_message_queue_get(ble->message_queue, &msg, FuriWaitForever) == FuriStatusOk) {
        //BLE_LOG_I("msg %d serv: %d", msg.type, msg.service_index);

        BleServiceObject* service = ble->services[msg.service_index];
        ble_service_run(service, &msg);
        // if(msg.type == BleCommandServiceRead) {
        //     FURI_LOG_I("MsgQueue", "msg %d serv: %d", msg.type, msg.service_index);

        //     BleServiceObject* service = ble->services[msg.service_index];
        //     ble_service_run(service);
        //     // BleServiceRun run = service->desc->run;
        //     // if(run) run(service);
        // } else if(msg.type == BleCommandServiceSetState) {
        //     if(msg.data[0] == BleServiceStateInitialization) {
        //         FURI_LOG_I("MsgQueue", "SetState");
        //     }
        // }
    } else
        FURI_LOG_W("MsgQueue", "Projebana cherga");
    // BLE_LOG_W("Projebana cherga");
}

void ble_custom_event_callback(uint32_t events, void* context) {
    Ble* instance = context;

    if(events == BleEventTypeFrameReceived) {
        const BleIntercomFrameGeneric* frame = &instance->mailbox;
        FURI_LOG_I(
            "Event",
            "Rcv type: %d serv: %d data_size: %d",
            frame->header.new_type,
            frame->header.service_index,
            frame->header.data_size);
        // BLE_LOG_D(
        //     "Rcv type: %d serv: %d data_size: %d",
        //     frame->header.new_type,
        //     frame->header.service_index,
        //     frame->header.data_size);

        // if(frame->header.service_index == BleIntercomServiceIndexBase) {
        //     ble_base_process_frame(instance);
        // } else {
        BleIntercomServiceIndex index = frame->header.service_index;
        if(index == BLE_SERVICES_COUNT) {
            index -= 1;
        }

        BleServiceObject* service = instance->services[index];
        //ble_service_process_frame(service, &instance->mailbox);
        ble_process_mailbox(service, &instance->mailbox);
        // }

        // BLE_LOG_D("Release frame_lock");
        FURI_LOG_D("Event", "Release frame_lock");
        furi_semaphore_release(instance->mailbox_lock);
    }
}

static void ble_backend_intercom_rx_callback(const void* data, size_t data_size, void* context) {
    furi_assert(context);
    furi_assert(data_size < MAX_BLE_INTERCOM_FRAME_SIZE);
    // BLE_LOG_D("input frame");
    FURI_LOG_I("RxIntercom", "input frame");
    Ble* instance = context;
    if(furi_semaphore_acquire(instance->mailbox_lock, 100) == FuriStatusOk) {
        memcpy(&instance->mailbox, data, data_size);
        furi_event_loop_set_custom_event(instance->event_loop, BleEventTypeFrameReceived);
    } else
        FURI_LOG_W("RxIntercom", "Projebany pakiet");
    // BLE_LOG_W("Projebany pakiet");
}

// static void ble_event_loop_on_start(void* context) {
//     BLE_LOG_W("ble_event_loop_on_start");
//     UNUSED(context);
//     // Ble* ble = context;
//     // ble->pending_service_index = 0;
//     // ble->state = BleServiceStateIdle;
//     // furi_delay_ms(10000);
//     // furi_event_loop_set_custom_event(ble->event_loop, BleEventTypeInit);
// }

static Ble* ble_alloc() {
    Ble* instance = malloc(sizeof(Ble));
    instance->state = BleServiceStateReset;
    instance->event_loop = furi_event_loop_alloc();
    instance->mailbox_lock = furi_semaphore_alloc(1, 1);
    instance->access_semaphore = furi_semaphore_alloc(1, 1);

    instance->message_queue = furi_message_queue_alloc(3, sizeof(BleMessage));

    furi_event_loop_set_custom_event_callback(
        instance->event_loop, ble_custom_event_callback, instance);

    furi_event_loop_subscribe_message_queue(
        instance->event_loop,
        instance->message_queue,
        FuriEventLoopEventIn,
        ble_event_loop_msg_queue_handler,
        instance);

    // furi_event_loop_pend_callback(instance->event_loop, ble_event_loop_on_start, instance);

    instance->intercom = furi_record_open(RECORD_INTERCOM);
    intercom_set_rx_callback(
        instance->intercom, IntercomChannelBle, ble_backend_intercom_rx_callback, instance);

    for(size_t i = 0; i < BLE_SERVICES_COUNT; i++) {
        instance->services[i] = ble_service_alloc(service_config[i], instance->message_queue);
        //ble_worker_create_service(&service_config[i], instance->message_queue);
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
    // message->lock = api_lock_alloc_locked();

    // furi_check(
    //     furi_semaphore_acquire(instance->access_semaphore, FuriWaitForever) == FuriStatusOk);

    instance->current_message = message;
    furi_event_loop_set_custom_event(instance->event_loop, BleEventTypeIncomingMessage);

    // api_lock_wait_unlock_and_free(message->lock);
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
