#include "ble_service_target.h"

static bool
    ble_service_command_allowed_by_state(const BleCommand command, const BleServiceState state) {
    UNUSED(state);
    UNUSED(command);
    return true;
}

bool ble_service_target_init(BleServiceObject* instance) {
    FURI_LOG_I(instance->desc->name, "target_init_u5");
    bool result = false;
    BleIntercomFrameGeneric* frame = (BleIntercomFrameGeneric*)instance->frame_buf;

    if(!ble_service_command_allowed_by_state(frame->header.command, instance->state)) return false;
    /* need to create some data to put in here */
    if(instance->desc->init(instance)) {
        FURI_LOG_I(instance->desc->name, "request start remote");

        size_t total_data_size = 0;
        uint8_t chars_count = instance->desc->char_count;
        for(size_t i = 0; i < chars_count; i++) {
            total_data_size += instance->chars[i]->data_size;
        }

        size_t total_config_size = sizeof(BleCharacteristicInitHeader) * chars_count +
                                   total_data_size + sizeof(BleCharacteristicCountType);
        BleServiceInitConfig* config = malloc(total_config_size);

        config->char_count = chars_count;
        uint8_t offset = 0;
        for(size_t i = 0; i < chars_count; i++) {
            BleCharacteristicObject* ch_obj = instance->chars[i];

            BleCharacteristicInit* char_init =
                (BleCharacteristicInit*)((uint8_t*)config->chars_config + offset);

            char_init->header.index = ch_obj->desc->intercom_index;
            char_init->header.data_size = ch_obj->data_size;
            FURI_LOG_D(instance->desc->name, "Char size: %d", ch_obj->data_size);

            const uint8_t* data = ch_obj->desc->get_data(ch_obj);
            memcpy(char_init->data, data, ch_obj->data_size);

            offset += (ch_obj->data_size + sizeof(BleCharacteristicInitHeader));
        }

        FURI_LOG_D(instance->desc->name, "Config size: %d", total_config_size);

        ble_service_prepare_frame(
            instance,
            BleIntercomFrameTypeRequest,
            BleCommandServiceInit,
            total_config_size,
            config);

        free(config);
        result = true;
    }

    return result;
}

static bool ble_service_on_init_response(BleServiceObject* instance) {
    ble_service_switch_state(instance, BleServiceStateReady);
    return true;
}

bool ble_service_target_process_response(BleServiceObject* instance) {
    BleIntercomFrameGeneric* frame = (BleIntercomFrameGeneric*)instance->frame_buf;
    const BleCommand command = frame->header.command;

    bool result = false;
    switch(command) {
    case BleCommandServiceInit:
        result = ble_service_on_init_response(instance);
        break;
    case BleCommandServiceRead:
        break;
    case BleCommandServiceWrite:
        break;
    case BleCommandServiceNotify:
        break;
    default:
        break;
    }

    return result;
}
