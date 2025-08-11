#include "ble.h"
#include "ble_common.h"
#include "service/ble_service.h"
#include "service/ble_service_config.h"

#include <intercom/intercom.h>
#if !defined(SI917)
#include <api_lock.h>
#else
#include "worker/ble_worker.h"
#endif

#define TAG "BLE"

typedef enum {
    BleEventTypeIncomingMessage,
    BleEventTypeFrameReceived,
} BleEventType;

struct Ble {
    BleServiceState state;

    FuriMessageQueue* message_queue;

    FuriSemaphore* mailbox_lock;
    BleIntercomFrameGeneric mailbox;

    FuriEventLoopTimer* heartbear_timer;
    FuriEventLoopTimer* test_timer;
    FuriMutex* ble_lock;

    FuriEventLoop* event_loop;
    Intercom* intercom;
    //--------------------------

    FuriSemaphore* access_semaphore;

    BleServiceObject* services[BLE_SERVICES_COUNT];
    BleMessage* current_message;
};

static void ble_heartbeat_handler(Ble* instance /* , BleServiceState remote_state */) {
    furi_assert(instance);
    BLE_LOG_D("ble_heartbeat_handler");

#if !defined(SI917)
    const BleIntercomFrameHeartbeat* heartbeat = (BleIntercomFrameHeartbeat*)&instance->mailbox;
    BleServiceState remote_state = heartbeat->state;
    if(instance->state == BleServiceStateReset && remote_state == BleServiceStateReset) {
        BLE_LOG_I("Enqueue services start...");
        for(size_t i = 0; i < BLE_SERVICES_COUNT; i++) {
            ble_service_eqnueue_init(instance->services[i]);
        }
        instance->state = BleServiceStateInitialization;
    }
#endif
}

static void ble_event_loop_msg_queue_handler(FuriEventLoopObject* object, void* context) {
    furi_assert(context);
    Ble* ble = context;
    furi_assert(object == ble->message_queue);

    BleMessage msg;
    if(furi_message_queue_get(ble->message_queue, &msg, FuriWaitForever) == FuriStatusOk) {
        BleServiceObject* service = ble->services[msg.service_index];
        ble_service_process(service, &msg);
    } else
        BLE_LOG_W("MsgQueue is full!");
}

void ble_custom_event_callback(uint32_t events, void* context) {
    Ble* instance = context;

    if(events == BleEventTypeFrameReceived) {
        const BleIntercomFrameGeneric* frame = &instance->mailbox;

        if(frame->header.frame_type != BleIntercomFrameTypeHeartbeat) {
            size_t frame_size = frame->header.data_size + sizeof(BleIntercomFrameHeader);
            BLE_LOG_D(
                "Rx Frame t: %d c: %d ds: %d fs: %d",
                frame->header.frame_type,
                frame->header.command,
                frame->header.data_size,
                frame_size);

            BleServiceIndex index = frame->header.service_index;
            BleServiceObject* service = instance->services[index];
            ble_service_process_mailbox(service, &instance->mailbox);
        } else {
            ble_heartbeat_handler(instance);
        }
        furi_semaphore_release(instance->mailbox_lock);
    }
}

static void ble_backend_intercom_rx_callback(const void* data, size_t data_size, void* context) {
    furi_assert(context);
    furi_assert(data_size < MAX_BLE_INTERCOM_FRAME_SIZE);
    Ble* instance = context;
    if(furi_semaphore_acquire(instance->mailbox_lock, 100) == FuriStatusOk) {
        memcpy(&instance->mailbox, data, data_size);
        furi_event_loop_set_custom_event(instance->event_loop, BleEventTypeFrameReceived);
    } else
        BLE_LOG_W("Packet lost!");
}

static void ble_heartbeat_timer_handler(void* context) {
    Ble* instance = context;
    BLE_LOG_D("Hearbeat");
    if(furi_semaphore_acquire(instance->mailbox_lock, 100) == FuriStatusOk) {
        BleIntercomFrameHeartbeat* frame = (BleIntercomFrameHeartbeat*)&instance->mailbox;

        frame->header.frame_type = BleIntercomFrameTypeHeartbeat;
        frame->header.data_size = sizeof(BleServiceState);
        frame->state = instance->state;

        size_t frame_size = sizeof(BleIntercomFrameHeartbeat);
        size_t tx = intercom_tx(instance->intercom, IntercomChannelBle, frame, frame_size, 100);
        furi_assert(tx == frame_size);

        furi_semaphore_release(instance->mailbox_lock);
    }
    furi_event_loop_timer_stop(instance->heartbear_timer);
}

#if defined(SI917)
static void test_timer_handler(void* context) {
    BLE_LOG_W("test_timer");
    UNUSED(context);
    ble_worker_test_after_init();
    ble_worker_start();
}

static void ble_event_loop_on_start(void* context) {
    UNUSED(context);
    BLE_LOG_W("ble_event_loop_on_start");
    // furi_delay_ms(10000);
    // Ble* ble = context;

    // BleServiceObject* service = ble->services[BleIntercomServiceIndexState];
    // ble_service_set_state(service, BleServiceStateInitialization);
}
#endif

static Ble* ble_alloc() {
    Ble* instance = malloc(sizeof(Ble));
    instance->state = BleServiceStateReset;
    instance->event_loop = furi_event_loop_alloc();
    instance->mailbox_lock = furi_semaphore_alloc(1, 1);
    instance->access_semaphore = furi_semaphore_alloc(1, 1);

    instance->message_queue = furi_message_queue_alloc(BLE_SERVICES_COUNT, sizeof(BleMessage));

    furi_event_loop_set_custom_event_callback(
        instance->event_loop, ble_custom_event_callback, instance);

    instance->heartbear_timer = furi_event_loop_timer_alloc(
        instance->event_loop,
        ble_heartbeat_timer_handler,
        FuriEventLoopTimerTypePeriodic,
        instance);
    furi_event_loop_timer_start(instance->heartbear_timer, 10000);

    furi_event_loop_subscribe_message_queue(
        instance->event_loop,
        instance->message_queue,
        FuriEventLoopEventIn,
        ble_event_loop_msg_queue_handler,
        instance);

    instance->intercom = furi_record_open(RECORD_INTERCOM);
    intercom_set_rx_callback(
        instance->intercom, IntercomChannelBle, ble_backend_intercom_rx_callback, instance);

    for(size_t i = 0; i < BLE_SERVICES_COUNT; i++) {
        instance->services[i] =
            ble_service_alloc(service_config[i], instance->message_queue, instance->intercom);
    }

#if defined(SI917)
    instance->test_timer = furi_event_loop_timer_alloc(
        instance->event_loop, test_timer_handler, FuriEventLoopTimerTypeOnce, instance);

    furi_event_loop_pend_callback(instance->event_loop, ble_event_loop_on_start, instance);
    furi_event_loop_timer_start(instance->test_timer, 20000);
    ble_worker_init();
#endif

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

    msg.type = BleCommandEnable;
    ble_send_message(ble, &msg);
    return msg.result;
}

bool ble_stop(Ble* ble) {
    BleMessage msg;
    msg.type = BleCommandDisable;
    ble_send_message(ble, &msg);
    return msg.result;
}
