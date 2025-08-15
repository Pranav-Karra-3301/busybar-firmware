#include "ble_service_target.h"

#define TAG "BleServiceU5"

static bool
    ble_service_command_allowed_by_state(const BleCommand command, const BleServiceState state) {
    UNUSED(state);
    UNUSED(command);
    return true;
}

static bool ble_service_target_init(BleServiceObject* instance) {
    BLE_LOG_D("%s - ble_service_target_init", instance->desc->name);

    bool result = false;
    /* need to create some data to put in here */
    if(instance->desc->init(instance)) {
        BLE_LOG_D("%s - request start remote", instance->desc->name);

        size_t total_data_size = 0;
        uint8_t chars_count = instance->desc->char_count;
        for(size_t i = 0; i < chars_count; i++) {
            total_data_size += ble_characteristic_get_data_size(instance->chars[i]);
        }

        size_t total_config_size = sizeof(BleCharacteristicDataHeader) * chars_count +
                                   total_data_size + sizeof(BleCharacteristicCountType);
        BleIntercomServiceData* config = malloc(total_config_size);

        config->char_count = chars_count;
        uint8_t offset = 0;
        for(size_t i = 0; i < chars_count; i++) {
            BleCharacteristicObject* ch_obj = instance->chars[i];

            BleCharacteristicData* char_init =
                (BleCharacteristicData*)((uint8_t*)config->chars_config + offset);

            offset += ble_characteristic_fill_update_struct(ch_obj, char_init);
        }

        BLE_LOG_D("%s - config size: %d", instance->desc->name, total_config_size);

        ble_service_prepare_send_intercom_frame(
            instance,
            BleIntercomFrameTypeRequest,
            BleCommandServiceInit,
            total_config_size,
            config);
        // ble_service_prepare_frame(
        //     instance,
        //     BleIntercomFrameTypeRequest,
        //     BleCommandServiceInit,
        //     total_config_size,
        //     config);

        free(config);
        result = true;
    }

    return result;
}

static bool ble_service_command_handler_init(
    BleServiceObject* instance,
    BleIntercomFrameType frame_type,
    size_t data_size,
    void* data) {
    UNUSED(data);
    UNUSED(data_size);

    bool result = false;
    if(frame_type == BleIntercomFrameTypeRequest) {
        BLE_LOG_D("Init request");
        result = ble_service_target_init(instance);
        // ble_service_send_intercom_frame(instance);
    } else {
        BLE_LOG_D("Init response");
        ble_service_switch_state(instance, BleServiceStateReady);
        result = true;
    }
    return result;
}

bool ble_service_target_execute(
    BleServiceObject* instance,
    BleIntercomFrameType frame_type,
    BleCommand command,
    size_t data_size,
    void* data) {
    BLE_LOG_D("%s - target_execute: %d", instance->desc->name, command);

    bool result = false;
    if(ble_service_command_allowed_by_state(command, instance->state)) {
        switch(command) {
        case BleCommandServiceInit:
            result = ble_service_command_handler_init(instance, frame_type, data_size, data);
            break;
        case BleCommandServiceRead:
            break;
        case BleCommandServiceWrite:
            break;
        // case BleCommandServiceNotify:
        //     result = ble_service_command_handler_notify(instance, frame_type, data_size, data);
        //     break;
        default:
            break;
        }
    }

    return result;
}
