#include "ble_service_target.h"
#include "../../worker/ble_worker.h"

bool ble_service_target_init(BleServiceObject* instance) {
    FURI_LOG_I(instance->desc->name, "target_init_917");

    BleServiceState state = BleServiceStateReady;

    const BleIntercomFrameServiceConfig* frame =
        (BleIntercomFrameServiceConfig*)instance->frame_buf;

    // if(instance->desc->init(instance))

    const BleServiceInitConfig* service_config = &frame->service_init;
    FURI_LOG_I(instance->desc->name, "Config char_count: %d", service_config->char_count);
    uint8_t offset = 0;

    for(size_t i = 0; i < service_config->char_count; i++) {
        //char->handle = ble_worker_register_characteristic(uiid, properties, data, data_size)

        const BleCharacteristicInit* char_init =
            (BleCharacteristicInit*)((uint8_t*)service_config->chars_config + offset);
        size_t data_size = char_init->header.data_size;

        FURI_LOG_I(
            instance->desc->name, "Char %d data_size: %d", char_init->header.index, data_size);
        BleCharacteristicObject* ch = instance->chars[char_init->header.index];
        ch->data_size = char_init->header.data_size;
        ch->data = malloc(ch->data_size);
        memcpy(ch->data, char_init->data, data_size);

        offset += (data_size + sizeof(BleCharacteristicInitHeader));
    }

    if(ble_worker_register_service(instance)) {
        ble_service_switch_state(instance, state);
    }

    ble_service_prepare_frame(
        instance, BleIntercomFrameTypeResponse, BleCommandServiceInit, 0, NULL);

    return true;
}

bool ble_service_target_process_response(BleServiceObject* instance) {
    UNUSED(instance);
    return true;
}

void ble_service_target_notify(
    BleServiceObject* instance,
    uint8_t ch_index,
    void* data,
    size_t data_size) {
    FURI_LOG_I(instance->desc->name, "ble_service_target_notify");
    BleCharacteristicObject* ch = instance->chars[ch_index];
    memcpy(ch->data, data, data_size);
    ble_worker_notify(ch->handle, data_size, data);
}
