#include "ble_service_target.h"

bool ble_service_target_init(BleServiceObject* instance) {
    FURI_LOG_I(instance->desc->name, "target_init_917");

    BleServiceState state = BleServiceStateReady;
    ble_service_switch_state(instance, state);
    BleCommandEvent cevt = {.command = BleCommandServiceInit};
    ble_service_send_intercom_frame(instance, BleIntercomFrameTypeResponse, cevt, 0, NULL);

    return true;
}
