#include "ble_service_target.h"

bool ble_service_target_init(BleServiceObject* instance) {
    FURI_LOG_I(instance->desc->name, "target_init_u5");
    bool result = false;

    if(instance->desc->init(instance /* need to create some data to put in here */)) {
        FURI_LOG_I(instance->desc->name, "request start remote");
        /* const BleIntercomFrameGeneric * frame = &instance->frame_buf; */
        BleCommandEvent cevt = {.command = BleCommandServiceInit};
        uint8_t data_test[5] = {5};
        ble_service_send_intercom_frame(instance, BleIntercomFrameTypeRequest, cevt, 5, data_test);
        result = true;
    }

    return result;
}
