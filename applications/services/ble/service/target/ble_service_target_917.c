#include "ble_service_target.h"
#include "../../worker/ble_worker.h"

#define TAG "BleService917"

static void ble_char_tx_done_cb(void* ctx) {
    BleCharacteristicObject* ch = ctx;
    uint16_t handle = ble_characteristic_get_handle(ch);
    const uint8_t cccd_value = ble_characteristic_get_cccd_value(ch);
    BLE_LOG_D("Upd resp, H: %04X, val: %02X", handle, cccd_value);
    ble_worker_receive_confirm(handle, cccd_value);
}

static void ble_characteristic_update_callback(size_t data_size, void* data, void* context) {
    BleCharacteristicObject* ch = context;
    const uint16_t handle = ble_characteristic_get_handle(ch);
    const uint8_t cccd_value = ble_characteristic_get_cccd_value(ch);
    ble_worker_send(handle, data_size, data, cccd_value);
}

static bool ble_service_command_handler_init(
    BleServiceObject* instance,
    BleIntercomFrameType frame_type,
    size_t data_size,
    void* data) {
    UNUSED(frame_type);
    bool result = false;
    do {
        if(data_size == 0) {
            ble_service_set_error(instance, "Empty init data");
            break;
        }

        if(!ble_service_parse_intercom_service_data(instance, data, NULL)) {
            ble_service_set_error(instance, "Failed to parse service data");
            break;
        }

        for(uint8_t i = 0; i < instance->config->char_count; i++) {
            BleCharacteristicObject* ch = instance->chars[i];
            ble_characteristic_register_tx_done_callback(ch, ble_char_tx_done_cb, ch);
            ble_characteristic_register_update_callback(
                ch, ble_characteristic_update_callback, ch);
        }

        if(!ble_worker_register_service(instance)) {
            ble_service_set_error(instance, "Failed to register service");
            break;
        }

        result = true;
        size_t total_size = 0;
        BleIntercomServiceData* config =
            ble_service_create_intercom_service_data_pack(instance, false, &total_size);

        if(config->char_count > 0) {
            ble_service_prepare_send_intercom_frame(
                instance,
                BleIntercomFrameTypeResponse,
                BleServiceCommandInit,
                result,
                total_size,
                config);
        }

        free(config);
    } while(false);
    return result;
}

static bool ble_service_command_handler_update(
    BleServiceObject* instance,
    BleIntercomFrameType frame_type,
    size_t data_size,
    void* data) {
    UNUSED(frame_type);
    UNUSED(data_size);

    bool result = false;
    do {
        if(!ble_service_parse_intercom_service_data(instance, data, NULL)) {
            BLE_LOG_W("Update command error");
            break;
        }

        size_t total_size = 0;
        BleIntercomServiceData* config =
            ble_service_create_intercom_service_data_pack(instance, true, &total_size);

        if(config->char_count > 0) {
            result = true;
            ble_service_prepare_send_intercom_frame(
                instance,
                BleIntercomFrameTypeRequest,
                BleServiceCommandUpdate,
                result,
                total_size,
                config);
        } else {
            BLE_LOG_W("not send, char = 0");
        }

        free(config);
    } while(false);

    return result;
}

static bool ble_service_command_handler_run(
    BleServiceObject* instance,
    BleIntercomFrameType frame_type,
    size_t data_size,
    void* data) {
    BLE_LOG_D("ble_service_command_handler_run");

    UNUSED(frame_type);
    UNUSED(data_size);
    UNUSED(data);

    bool result = false;
    do {
        size_t total_size = 0;
        BleIntercomServiceData* config =
            ble_service_create_intercom_service_data_pack(instance, true, &total_size);

        BLE_LOG_D("%s - config size: %d", instance->config->name, total_size);
        result = true;

        ble_service_prepare_send_intercom_frame(
            instance,
            BleIntercomFrameTypeRequest,
            BleServiceCommandUpdate,
            result,
            total_size,
            config);

        free(config);

    } while(false);
    return result;
}

bool ble_service_target_execute(
    BleServiceObject* instance,
    BleIntercomFrameType frame_type,
    BleServiceCommandEnum command,
    size_t data_size,
    void* data) {
    BLE_LOG_D("%s - target_execute: %d", instance->config->name, command);

    bool result = false;
    switch(command) {
    case BleServiceCommandInit:
        result = ble_service_command_handler_init(instance, frame_type, data_size, data);
        break;
    case BleServiceCommandRun:
        ble_service_command_handler_run(instance, frame_type, data_size, data);
        break;
    case BleServiceCommandUpdate:
        result = ble_service_command_handler_update(instance, frame_type, data_size, data);
        break;
    default:
        furi_crash("Unknown command");
        break;
    }

    return result;
}
