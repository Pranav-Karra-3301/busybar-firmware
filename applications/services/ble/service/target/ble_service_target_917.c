#include "ble_service_target.h"
#include "../../worker/ble_worker.h"

#define TAG "BleService917"

static bool
    ble_service_command_allowed_by_state(const BleCommand command, const BleServiceState state) {
    UNUSED(state);
    UNUSED(command);
    return true;
}

static bool ble_service_target_init(BleServiceObject* instance, size_t data_size, void* data) {
    BLE_LOG_D("%s - ble_service_target_init", instance->desc->name);

    BleServiceState state = BleServiceStateReady;

    // const BleIntercomFrameServiceConfig* frame =
    //     (BleIntercomFrameServiceConfig*)instance->frame_buf;

    // if(instance->desc->init(instance))

    if(data_size == 0) return false;

    const BleIntercomServiceData* service_config = data;
    BLE_LOG_D("%s - config char_count: %d", instance->desc->name, service_config->char_count);
    uint8_t offset = 0;

    for(size_t i = 0; i < service_config->char_count; i++) {
        const BleCharacteristicData* char_init =
            (BleCharacteristicData*)((uint8_t*)service_config->chars_config + offset);
        size_t data_size = char_init->header.data_size;

        BLE_LOG_D(
            "%s - char: %d data_size: %d",
            instance->desc->name,
            char_init->header.index,
            data_size);
        BleCharacteristicObject* ch = instance->chars[char_init->header.index];
        ble_characteristic_set_data(ch, char_init->data, data_size);

        offset += (data_size + sizeof(BleCharacteristicDataHeader));
    }

    if(ble_worker_register_service(instance)) {
        ble_service_switch_state(instance, state);
    }

    ble_service_prepare_send_intercom_frame(
        instance, BleIntercomFrameTypeResponse, BleCommandServiceInit, 0, NULL);
    // ble_service_prepare_frame(
    //     instance, BleIntercomFrameTypeResponse, BleCommandServiceInit, 0, NULL);

    return true;
}

///TODO: Deal with character index!!!
void ble_service_target_notify(
    BleServiceObject* instance,
    uint8_t ch_index,
    void* data,
    size_t data_size) {
    BLE_LOG_D("%s - ble_service_target_notify", instance->desc->name);
    BleCharacteristicObject* ch = instance->chars[ch_index];
    ble_characteristic_set_data(ch, data, data_size);
    const uint16_t handle = ble_characteristic_get_handle(ch);
    ble_worker_notify(handle, data_size, data);
}

static bool ble_service_command_handler_init(
    BleServiceObject* instance,
    BleIntercomFrameType frame_type,
    size_t data_size,
    void* data) {
    bool result = false;
    if(frame_type == BleIntercomFrameTypeRequest) {
        BLE_LOG_D("Init request");
        result = ble_service_target_init(instance, data_size, data);
        // ble_service_send_intercom_frame(instance);
    } else {
        BLE_LOG_D("Init response");
    }
    return result;
}

///TODO: Deal with character index!!!
// static bool
//     ble_service_command_handler_notify(BleServiceObject* instance, BleIntercomFrameGeneric* frame) {
//     BleCharacteristicData* ch_data = (BleCharacteristicData*)frame->data;
//     ble_service_target_notify(
//         instance, ch_data->header.index, ch_data->data, ch_data->header.data_size);
//     return true;
// }

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
        case BleCommandServiceRun:
            BLE_LOG_W("Run not implemented on 917");
            break;
        case BleCommandServiceRead:
            break;
        case BleCommandServiceWrite:
            break;
        ///TODO: think of completely remove Notify command!
        // case BleCommandServiceNotify:
        //     result = ble_service_command_handler_notify(instance, frame);
        //     break;
        default:
            break;
        }
    }

    return result;
}
