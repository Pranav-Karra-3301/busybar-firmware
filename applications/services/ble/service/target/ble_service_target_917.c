#include "ble_service_target.h"

bool ble_service_target_init(BleServiceObject* instance) {
    FURI_LOG_I(instance->desc->name, "target_init_917");

    BleServiceState state = BleServiceStateReady;

    FuriString* buf = furi_string_alloc();

    const BleIntercomFrameServiceConfig* frame =
        (BleIntercomFrameServiceConfig*)instance->frame_buf;

    const BleServiceInitConfig* service_config = &frame->service_init;
    FURI_LOG_I(instance->desc->name, "Config char_count: %d", service_config->char_count);
    uint8_t offset = 0;
    for(size_t i = 0; i < service_config->char_count; i++) {
        const BleCharacteristicInit* char_init =
            (BleCharacteristicInit*)((uint8_t*)service_config->chars_config + offset);

        size_t data_size = char_init->header.data_size;
        FURI_LOG_I(instance->desc->name, "Char data_size: %d", data_size);

        furi_string_reset(buf);
        for(size_t j = 0; j < data_size; j++) {
            furi_string_cat_printf(buf, "%02X", char_init->data[j]);
        }
        FURI_LOG_I(instance->desc->name, "Data: %s", furi_string_get_cstr(buf));

        offset += (data_size + sizeof(BleCharacteristicInitHeader));
    }
    furi_string_free(buf);

    ble_service_switch_state(instance, state);

    ble_service_prepare_frame(
        instance, BleIntercomFrameTypeResponse, BleCommandServiceInit, 0, NULL);

    return true;
}

bool ble_service_target_process_response(BleServiceObject* instance) {
    UNUSED(instance);
    return true;
}
