#include "ble_command.h"

#define TAG "BLE_917"

void ble_command_handler_enable() {
}

void ble_command_handler_disable() {
}

void ble_command_handler_get_status(Ble* instance, BleIntercomFrameStatus* frame) {
    if(frame->header.frame_type == BleIntercomFrameTypeResponse) {
        BLE_LOG_W("No need response");
    } else {
        BLE_LOG_D("GetStatus response");
        frame->header.frame_type = BleIntercomFrameTypeResponse;
        frame->header.data_size = sizeof(BleServiceState);
        frame->state = instance->state;

        size_t frame_size = sizeof(BleIntercomFrameStatus);
        size_t tx = intercom_tx(instance->intercom, IntercomChannelBle, frame, frame_size, 100);
        furi_assert(tx == frame_size);
    }
}
