#include "ble_service.h"

// #include <furi_hal.h>
// #include <furi_hal_nvm.h>
#include <furi_hal_info.h>

#define TAG "BleService"

uint8_t write_cnt = 0;
FuriString* results[3];

static void device_info_callback(const char* key, const char* value, bool last, void* context) {
    UNUSED(last);
    UNUSED(context);

    BLE_LOG_W("Getting device info");
    FuriString* key_str = furi_string_alloc_set_str(key);

    if(furi_string_equal_str(key_str, "u5_firmware_commit")) {
        results[2] = furi_string_alloc_printf(value);
    } else if(furi_string_equal_str(key_str, "u5_hardware_uid")) {
        results[1] = furi_string_alloc_printf(value);
    } else if(furi_string_equal_str(key_str, "u5_firmware_target")) {
        results[0] = furi_string_alloc_printf(value);
    }

    furi_string_free(key_str);
    // printf("%-30s: %s\r\n", key, value);
}

bool ble_service_device_info_init(void* context, BleIntercomFrameGeneric* frame) {
    furi_assert(context);
    furi_assert(frame);
    // BleServiceObject* instance = context;

    BLE_LOG_W("device_info_init");
    furi_hal_info_get(device_info_callback, '_', NULL);
    BleIntercomFrameServiceConfig* frd = (BleIntercomFrameServiceConfig*)frame;

    frd->header.type = BleRequestTypeInit;
    frd->header.service_index = BleIntercomServiceIndexDeviceInfo;
    frd->header.data_size = sizeof(BleCharSize) * 3 + sizeof(frd->char_count);

    frd->char_count = 3;

    frd->chars_config[0].intercom_index = BleSrvDeviceInfoCharacterIndexSerialNumber;
    frd->chars_config[0].data_size = furi_string_size(results[0]);

    frd->chars_config[1].intercom_index = BleSrvDeviceInfoCharacterIndexHardwareRevision;
    frd->chars_config[1].data_size = furi_string_size(results[1]);

    frd->chars_config[2].intercom_index = BleSrvDeviceInfoCharacterIndexSoftwareRevision;
    frd->chars_config[2].data_size = furi_string_size(results[2]);

    return true;
}

void ble_service_device_info_on_response(void* context, BleIntercomFrameGeneric* response) {
    furi_assert(context);
    furi_assert(response);
    BLE_LOG_W("device_info_on_response");
    BleServiceObject* instance = context;

    furi_semaphore_release(instance->access_lock);

    if(write_cnt < 3) {
        ble_service_common_write(
            context,
            write_cnt,
            furi_string_get_cstr(results[write_cnt]),
            furi_string_size(results[write_cnt]),
            //strlen(test[write_cnt]),
            response);
        write_cnt++;
    }
}

//==========================================================

static const BleCharacteristicDescriptor battery_service_characteristics[] = {
    {
        .intercom_index = 0,
        .name = "Battery Level",
        .uuid = {.Char_UUID_16 = 0x2A19},
        .uuid_size = 2,
        .data_size = sizeof(uint8_t),
        // .char_properties = RSI_BLE_ATT_PROPERTY_READ | RSI_BLE_ATT_PROPERTY_NOTIFY,
    },
    {
        .intercom_index = 1,
        .name = "Battery Status",
        .uuid = {.Char_UUID_16 = 0x2BED},
        .uuid_size = 2,
        .data_size = sizeof(BatteryStatusInfo),
        // .char_properties = RSI_BLE_ATT_PROPERTY_READ | RSI_BLE_ATT_PROPERTY_NOTIFY,
    },
};

//==========================================================
#define UART_SERVICE_UUID \
    {0x6E, 0x40, 0x00, 0x01, 0xB5, 0xA3, 0xF3, 0x93, 0xE0, 0xA9, 0xE5, 0x0E, 0x24, 0xDC, 0xCA, 0x9E}
#define UART_RX_CHAR_UUID \
    {0x6E, 0x40, 0x00, 0x02, 0xB5, 0xA3, 0xF3, 0x93, 0xE0, 0xA9, 0xE5, 0x0E, 0x24, 0xDC, 0xCA, 0x9E}
#define UART_TX_CHAR_UUID \
    {0x6E, 0x40, 0x00, 0x03, 0xB5, 0xA3, 0xF3, 0x93, 0xE0, 0xA9, 0xE5, 0x0E, 0x24, 0xDC, 0xCA, 0x9E}

static const BleCharacteristicDescriptor nordic_uart_service_characteristics[] = {
    {
        .intercom_index = 0,
        .name = "Uart Rx",
        .uuid = {.Char_UUID_128 = UART_RX_CHAR_UUID},
        .uuid_size = 16,
        .data_size = sizeof(2),
        // .char_properties = RSI_BLE_ATT_PROPERTY_READ | RSI_BLE_ATT_PROPERTY_WRITE,
    },
    {
        .intercom_index = 1,
        .name = "Uart Tx",
        .uuid = {.Char_UUID_128 = UART_TX_CHAR_UUID},
        .uuid_size = 16,
        .data_size = sizeof(2),
        // .char_properties = RSI_BLE_ATT_PROPERTY_READ | RSI_BLE_ATT_PROPERTY_NOTIFY,
    },
};
//==========================================================
const BleCharacteristicDescriptor device_info_service_characteristics[] = {
    {
        .intercom_index = BleSrvDeviceInfoCharacterIndexSerialNumber,
        .name = "Serial Number",
        .uuid = {.Char_UUID_16 = 0x2A25},
        .uuid_size = 2,
        // .char_properties = RSI_BLE_ATT_PROPERTY_READ,
    },
    {
        .intercom_index = BleSrvDeviceInfoCharacterIndexHardwareRevision,
        .name = "Hardware Revision",
        .uuid = {.Char_UUID_16 = 0x2A27},
        .uuid_size = 2,
        // .char_properties = RSI_BLE_ATT_PROPERTY_READ,
    },
    {
        .intercom_index = BleSrvDeviceInfoCharacterIndexSoftwareRevision,
        .name = "Software Revision",
        .uuid = {.Char_UUID_16 = 0x2A26},
        .uuid_size = 2,
        // .char_properties = RSI_BLE_ATT_PROPERTY_READ,
    },
};
//==========================================================
const BleServiceDescriptor service_config[] = {
    [BleIntercomServiceIndexDeviceInfo] =
        {
            .name = "Device Information",
            .uuid = {.Char_UUID_16 = 0x180A},
            .uuid_size = 2,
            .index = BleIntercomServiceIndexDeviceInfo,
            .init_method = BleServiceInitMethodRemote,
            .char_count = COUNT_OF(device_info_service_characteristics),
            .char_descriptors = device_info_service_characteristics,
            .init = ble_service_device_info_init,
            .on_response = ble_service_device_info_on_response,
        },
    [BleIntercomServiceIndexBattery] =
        {
            .name = "Battery Service",
            .uuid = {.Char_UUID_16 = 0x180F},
            .uuid_size = 2,
            .index = BleIntercomServiceIndexBattery,
            .init_method = BleServiceInitMethodLocal,
            .char_count = COUNT_OF(battery_service_characteristics),
            .char_descriptors = battery_service_characteristics,
        },
    [BleIntercomServiceIndexUart] =
        {
            .name = "Nordic UART",
            .uuid = {.Char_UUID_128 = UART_SERVICE_UUID},
            .uuid_size = 16,
            .index = BleIntercomServiceIndexUart,
            .init_method = BleServiceInitMethodLocal,
            .char_count = COUNT_OF(nordic_uart_service_characteristics),
            .char_descriptors = nordic_uart_service_characteristics,
        },
};

BleCharacteristicObject* ble_characteristic_alloc(const BleCharacteristicDescriptor* desc) {
    // furi_assert(desc);
    // furi_assert(service_handler);
    // furi_assert(desc->data_size);
    // furi_assert(out_handle);

    // uuid_t uuid = {0};
    // ble_prepare_uuid(&desc->uuid, desc->uuid_size, &uuid);

    BleCharacteristicObject* instance = malloc(sizeof(BleCharacteristicObject));
    instance->desc = desc;

    return instance;
}

BleServiceObject* ble_worker_create_service(
    const BleServiceDescriptor* service_config,
    FuriSemaphore* access,
    Intercom* intercom) {
    BleServiceObject* instance = malloc(sizeof(BleServiceObject));
    BLE_LOG_I("Create %s service", service_config->name);

    instance->desc = service_config;
    instance->intercom = intercom;
    instance->access_lock = access;
    instance->chars = malloc(sizeof(BleCharacteristicObject*) * service_config->char_count);

    for(size_t i = 0; i < service_config->char_count; i++) {
        const BleCharacteristicDescriptor* config = &service_config->char_descriptors[i];
        BleCharacteristicObject* ble_char = ble_characteristic_alloc(config);
        instance->chars[config->intercom_index] = ble_char;
    }

    instance->state = BleServiceStateIdle;
    return instance;
}

bool ble_service_common_init(void* context, BleIntercomFrameGeneric* frame) {
    furi_assert(context);
    furi_assert(frame);
    BleServiceObject* service = context;

    BLE_LOG_I("Init %s", service->desc->name);

    BleServiceInit init_cb = service->desc->init;
    // BleServiceInit init_cb = service->desc->init;
    // if(init_cb == NULL) {
    //     BLE_LOG_W("Skip no cb");
    //     instance->pending_service_index++;
    //     continue;
    // }
    if(init_cb == NULL) return false;

    if(init_cb(service, frame)) {
        furi_check(furi_semaphore_acquire(service->access_lock, FuriWaitForever) == FuriStatusOk);
        size_t data_size = frame->header.data_size + sizeof(BleIntercomFrameHeader);
        size_t tx_size = intercom_tx(service->intercom, IntercomChannelBle, frame, data_size, 100);
        furi_assert(data_size == tx_size);
    }
    return true;
}

void ble_service_common_write(
    void* context,
    uint16_t char_index,
    const void* data,
    size_t data_size,
    BleIntercomFrameGeneric* frame) {
    BleServiceObject* service = context;

    BleIntercomFrameCharData* char_frame = (BleIntercomFrameCharData*)frame;
    char_frame->header.service_index = service->desc->index;
    char_frame->header.data_size = data_size;
    char_frame->header.type = BleRequestTypeWrite;
    char_frame->char_index = char_index;
    memcpy(char_frame->data, data, data_size);

    BLE_LOG_W("Trying to take in response");

    furi_check(furi_semaphore_acquire(service->access_lock, FuriWaitForever) == FuriStatusOk);
    BLE_LOG_W("Trying to write in response");
    size_t ds =
        frame->header.data_size + sizeof(BleIntercomFrameHeader) + sizeof(char_frame->char_index);

    size_t tx_size = intercom_tx(service->intercom, IntercomChannelBle, frame, ds, 100);
    furi_assert(ds == tx_size);
}
