#include "ble_i.h"

#define TAG "BleAPI"

static void ble_send_message(Ble* instance, BleMessage* message) {
    message->lock = api_lock_alloc_locked();

    instance->current_message = message;
    furi_event_loop_set_custom_event(instance->event_loop, BleEventTypeIncomingMessage);

    api_lock_wait_unlock_and_free(message->lock);
}

bool ble_start(Ble* ble) {
    BleMessage msg = {0};
    msg.header.frame_type = BleIntercomFrameTypeRequest;
    msg.header.command = BleCommandGetStatus;
    msg.header.data_size = 0;
    ble_send_message(ble, &msg);

    if(msg.result) {
        furi_delay_ms(2000);
        msg.header.frame_type = BleIntercomFrameTypeRequest;
        msg.header.command = BleCommandEnable;
        msg.header.data_size = 0;
        ble_send_message(ble, &msg);
    }

    return msg.result;
}

bool ble_stop(Ble* ble) {
    BleMessage msg = {0};
    msg.header.frame_type = BleIntercomFrameTypeRequest;
    msg.header.command = BleCommandDisable;
    msg.header.data_size = 0;
    ble_send_message(ble, &msg);
    return msg.result;
}
