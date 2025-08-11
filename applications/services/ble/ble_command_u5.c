#include "ble_command.h"

#define TAG "BLE_U5"

void ble_command_handler_enable() {
}

void ble_command_handler_disable() {
}

void ble_command_handler_get_status(Ble* instance, BleIntercomFrameStatus* frame) {
    if(frame->header.frame_type == BleIntercomFrameTypeRequest) {
        BLE_LOG_W("Skip status request");
    } else {
        if(instance->state == BleServiceStateReset && frame->state == BleServiceStateReset) {
            BLE_LOG_I("Enqueue services start...");
            for(size_t i = 0; i < BLE_SERVICES_COUNT; i++) {
                ble_service_eqnueue_init(instance->services[i]);
            }
            instance->state = BleServiceStateInitialization;
            furi_event_loop_timer_stop(instance->init_timer);
        }
    }
}
