#include "ble_service_battery_i.h"

#define TAG "BleBattery"

///TODO: move this to character separate logic
static const uint8_t* battery_character_get_data(void* obj) {
    furi_assert(obj);
    BleCharacteristicObject* instance = obj;
    return instance->data;
}

//==========================================================
static const BleCharacteristicDescriptor battery_service_characteristics[] = {
    {
        .intercom_index = BleSrvBatteryCharacterIndexBatteryLevel,
        .name = "Battery Level",
        .data_size = sizeof(uint8_t),
        .get_data = battery_character_get_data,
#if defined(SI917)
        .uuid = {.Char_UUID_16 = 0x2A19},
        .uuid_size = 2,
        .char_properties = BLE_ATT_PROPERTY_READ | BLE_ATT_PROPERTY_NOTIFY,
#endif
    },
    {
        .intercom_index = BleSrvBatteryCharacterIndexBatteryStatus,
        .name = "Battery Status",
        .data_size = sizeof(BatteryStatusInfo),
        .get_data = battery_character_get_data,
#if defined(SI917)
        .uuid = {.Char_UUID_16 = 0x2BED},
        .uuid_size = 2,
        .char_properties = BLE_ATT_PROPERTY_READ | BLE_ATT_PROPERTY_NOTIFY,
#endif
    },
};

const BleServiceDescriptor ble_service_config_battery = {
    .name = "Battery Service",
#if defined(SI917)
    .uuid = {.Char_UUID_16 = 0x180F},
    .uuid_size = 2,
#endif
    .init = ble_service_battery_init,
    .index = BleIntercomServiceIndexBattery,
    .init_method = BleServiceInitMethodLocal,
    .char_count = COUNT_OF(battery_service_characteristics),
    .char_descriptors = battery_service_characteristics,
};
