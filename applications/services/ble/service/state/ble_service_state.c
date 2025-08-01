#include "ble_service_state.h"

#include <stdint.h>

#define TAG "BleState"

static bool ble_service_state_init(void* context, BleIntercomFrameGeneric* frame) {
    furi_assert(context);
    furi_assert(frame);

    BLE_LOG_W("state_init");

    return true;
}

const BleServiceDescriptor ble_service_state = {
    .name = "State Service",
    .index = BleIntercomServiceIndexState,
    .init_method = BleServiceInitMethodLocal,
    .char_count = 0,
    .char_descriptors = NULL,
    .init = ble_service_state_init,
    // .on_response = ble_service_battery_on_response,

};
