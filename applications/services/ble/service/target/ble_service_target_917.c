#include "ble_service_target.h"
#include "../../worker/ble_worker.h"

#define TAG "BleService917"

// static bool ble_service_target_init(BleServiceObject* instance, size_t data_size, void* data) {
//     BLE_LOG_I("%s - ble_service_target_init", instance->config->name);

//     do {
//         if(data_size == 0) {
//             ble_service_set_error(instance, "Empty init data");
//             break;
//         }

//         if(!ble_service_parse_intercom_service_data(instance, data, NULL)) {
//             ble_service_set_error(instance, "Failed to parse service data");
//             break;
//         }

//         if(!ble_worker_register_service(instance)) {
//             ble_service_set_error(instance, "Failed to register service");
//             break;
//         }
//         instance->ready = true;
//     } while(false);

//     BLE_LOG_I("%s - %s", instance->config->name, instance->ready ? "Ready" : "Not ready");

//     ble_service_prepare_send_intercom_frame(
//         instance,
//         BleIntercomFrameTypeResponse,
//         BleServiceCommandInit,
//         instance->ready,
//         furi_string_size(instance->error),
//         furi_string_get_cstr(instance->error));

//     return true;
// }

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
            // if(instance->config->index == BleServiceIndexBattery) {
            //     BLE_LOG_W("BKPT!!!");
            //     __BKPT(0);
            // }

            BLE_LOG_I("Send init response");
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

// static bool ble_service_update_request(BleServiceObject* instance, size_t data_size, void* data) {
//     bool result = false;

//     // if(instance->config->index == BleServiceIndexNordicUart) {
//     //     FuriString* str = furi_string_alloc();

//     //     const BleIntercomServiceData* service_config = data;
//     //     furi_string_printf(str, "Ch_cnt: %d ", service_config->char_count);
//     //     size_t offset = 0;
//     //     for(uint8_t i = 0; i < service_config->char_count; i++) {
//     //         const BleCharacteristicData* ch =
//     //             (BleCharacteristicData*)service_config->chars_config + offset;

//     //         furi_string_cat_printf(
//     //             str, "Ch_ind: %d, Ch_sz: %d", ch->header.index, ch->header.data_size);

//     //         offset += ch->header.data_size + sizeof(BleCharacteristicDataHeader);
//     //     }

//     //     BLE_LOG_I("In_Request[%ld]: %s", instance->frame_num, furi_string_get_cstr(str));
//     //     furi_string_free(str);
//     // }

//     do {
//         if(data_size == 0) {
//             ble_service_set_error(instance, "Empty data");
//             break;
//         }

//         if(!ble_service_parse_intercom_service_data(
//                instance, data, ble_service_update_characteristic_extra_action)) {
//             ble_service_set_error(instance, "Failed to parse service data");
//             break;
//         }
//         result = true;
//     } while(false);

//     // if(instance->config->index == BleServiceIndexNordicUart) {
//     //     FuriString* str = furi_string_alloc();

//     //     const BleIntercomServiceData* service_config = data;
//     //     furi_string_printf(str, "Ch_cnt: %d ", service_config->char_count);
//     //     size_t offset = 0;
//     //     for(uint8_t i = 0; i < service_config->char_count; i++) {
//     //         const BleCharacteristicData* ch =
//     //             (BleCharacteristicData*)service_config->chars_config + offset;

//     //         furi_string_cat_printf(
//     //             str, "Ch_ind: %d, Ch_sz: %d", ch->header.index, ch->header.data_size);

//     //         offset += ch->header.data_size + sizeof(BleCharacteristicDataHeader);
//     //     }

//     //     BLE_LOG_I("Out_Response[%ld]: %s", instance->frame_num, furi_string_get_cstr(str));
//     //     furi_string_free(str);
//     // }

//     ble_service_prepare_send_intercom_frame(
//         instance,
//         BleIntercomFrameTypeResponse,
//         BleServiceCommandUpdate,
//         result,
//         furi_string_size(instance->error),
//         furi_string_get_cstr(instance->error));

//     return true;
// }

// static bool ble_service_update_response(BleServiceObject* instance, size_t data_size, void* data) {
//     UNUSED(data_size);

//     const BleIntercomServiceData* service_config = data;
//     uint8_t offset = 0;

//     if(instance->config->index == BleServiceIndexNordicUart) {
//         FuriString* str = furi_string_alloc();

//         furi_string_printf(str, "Ch_cnt: %d ", service_config->char_count);
//         size_t offset = 0;
//         for(uint8_t i = 0; i < service_config->char_count; i++) {
//             const BleCharacteristicData* ch =
//                 (BleCharacteristicData*)service_config->chars_config + offset;

//             furi_string_cat_printf(
//                 str, "Ch_ind: %d, Ch_sz: %d", ch->header.index, ch->header.data_size);

//             offset += ch->header.data_size + sizeof(BleCharacteristicDataHeader);
//         }

//         BLE_LOG_I(
//             "In_Response[%ld]: %s", *(uint32_t*)instance->frame_buf, furi_string_get_cstr(str));
//         furi_string_free(str);
//     }

//     for(size_t i = 0; i < service_config->char_count; i++) {
//         const BleCharacteristicData* char_init =
//             (BleCharacteristicData*)((uint8_t*)service_config->chars_config + offset);

//         BleCharacteristicObject* ch = instance->chars[char_init->header.index];
//         uint16_t handle = ble_characteristic_get_handle(ch);
//         const uint8_t cccd_value = ble_characteristic_get_cccd_value(ch);
//         BLE_LOG_D("Upd resp, H: %04X, val: %02X", handle, cccd_value);
//         ble_worker_receive_confirm(handle, cccd_value);
//     }

//     // ble_service_update_processing_end(instance);
//     return true;
// }

// static bool ble_service_command_handler_update(
//     BleServiceObject* instance,
//     BleIntercomFrameType frame_type,
//     size_t data_size,
//     void* data) {
//     if(frame_type == BleIntercomFrameTypeRequest) {
//         return ble_service_update_request(instance, data_size, data);
//     } else {
//         return ble_service_update_response(instance, data_size, data);
//     }
// }

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

        if(instance->config->index == BleServiceIndexBattery) {
            FuriString* str = furi_string_alloc();

            furi_string_printf(str, "Ch_cnt: %d ", config->char_count);
            size_t offset = 0;
            for(uint8_t i = 0; i < config->char_count; i++) {
                const BleCharacteristicData* ch =
                    (BleCharacteristicData*)config->chars_config + offset;

                furi_string_cat_printf(
                    str, "Ch_ind: %d, Ch_sz: %d", ch->header.index, ch->header.data_size);

                offset += ch->header.data_size + sizeof(BleCharacteristicDataHeader);
            }

            BLE_LOG_I("Out_Request[%ld]: %s", instance->frame_num, furi_string_get_cstr(str));
            furi_string_free(str);
        }

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
