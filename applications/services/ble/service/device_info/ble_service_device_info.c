#include "ble_service_device_info.h"

#include <furi_hal_info.h>
#include <stdint.h>

#define TAG "BleDevInfo"

typedef enum {
    BleSrvDeviceInfoCharacterIndexSerialNumber,
    BleSrvDeviceInfoCharacterIndexHardwareRevision,
    BleSrvDeviceInfoCharacterIndexSoftwareRevision,
} BleSrvDeviceInfoCharacterIndex;

static void device_info_callback(const char* key, const char* value, bool last, void* context) {
    UNUSED(last);
    UNUSED(context);
    UNUSED(value);

    FuriString* key_str = furi_string_alloc_set_str(key);

    // if(furi_string_equal_str(key_str, "u5_firmware_commit")) {
    //     results[2] = furi_string_alloc_printf(value);
    // } else if(furi_string_equal_str(key_str, "u5_hardware_uid")) {
    //     results[1] = furi_string_alloc_printf(value);
    // } else if(furi_string_equal_str(key_str, "u5_firmware_target")) {
    //     results[0] = furi_string_alloc_printf(value);
    // }

    furi_string_free(key_str);
}

static bool ble_service_device_info_init(void* context, BleIntercomFrameGeneric* frame) {
    furi_assert(context);
    furi_assert(frame);

    BLE_LOG_W("device_info_init");
    furi_hal_info_get(device_info_callback, '_', NULL);
    BleIntercomFrameServiceConfig* frd = (BleIntercomFrameServiceConfig*)frame;

    frd->header.type = BleRequestTypeInit;
    frd->header.service_index = BleIntercomServiceIndexDeviceInfo;
    frd->header.data_size = sizeof(BleCharSize) * 3 + sizeof(frd->char_count);

    frd->char_count = 3;

    frd->chars_config[0].intercom_index = BleSrvDeviceInfoCharacterIndexSerialNumber;
    // frd->chars_config[0].data_size = furi_string_size(results[0]);

    frd->chars_config[1].intercom_index = BleSrvDeviceInfoCharacterIndexHardwareRevision;
    // frd->chars_config[1].data_size = furi_string_size(results[1]);

    frd->chars_config[2].intercom_index = BleSrvDeviceInfoCharacterIndexSoftwareRevision;
    // frd->chars_config[2].data_size = furi_string_size(results[2]);

    return true;
}

//==========================================================
static const BleCharacteristicDescriptor device_info_service_characteristics[] = {
    {
        .intercom_index = BleSrvDeviceInfoCharacterIndexSerialNumber,
        .name = "Serial Number",
#if defined(SI917)
        .uuid = {.Char_UUID_16 = 0x2A25},
        .uuid_size = 2,
#endif
        // .char_properties = RSI_BLE_ATT_PROPERTY_READ,
    },
    {
        .intercom_index = BleSrvDeviceInfoCharacterIndexHardwareRevision,
        .name = "Hardware Revision",
#if defined(SI917)
        .uuid = {.Char_UUID_16 = 0x2A27},
        .uuid_size = 2,
#endif
        // .char_properties = RSI_BLE_ATT_PROPERTY_READ,
    },
    {
        .intercom_index = BleSrvDeviceInfoCharacterIndexSoftwareRevision,
        .name = "Software Revision",
#if defined(SI917)
        .uuid = {.Char_UUID_16 = 0x2A26},
        .uuid_size = 2,
#endif
        // .char_properties = RSI_BLE_ATT_PROPERTY_READ,
    },
};

const BleServiceDescriptor ble_service_config_device_info = {
    .name = "Device Information",
#if defined(SI917)
    .uuid = {.Char_UUID_16 = 0x180A},
    .uuid_size = 2,
#endif
    .index = BleIntercomServiceIndexDeviceInfo,
    .init_method = BleServiceInitMethodRemote,
    .char_count = COUNT_OF(device_info_service_characteristics),
    .char_descriptors = device_info_service_characteristics,
    .init = ble_service_device_info_init,
    // .run = ble_service_device_info_run,
    // .on_response = ble_service_device_info_on_response,
};
