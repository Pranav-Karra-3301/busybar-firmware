#include "ble_service_battery.h"

#include <stdint.h>

#define TAG "BleBattery"

typedef enum {
    BleSrvBatteryCharacterIndexBatteryLevel,
    BleSrvBatteryCharacterIndexBatteryStatus,
} BleSrvBatteryCharacterIndex;

typedef struct {
    uint8_t flags;
    uint16_t power_state;
    uint8_t battery_level;
} FURI_PACKED BatteryStatusInfo;

static bool ble_service_battery_init(void* context) {
    furi_assert(context);

    BLE_LOG_W("battery_init");

    return true;
}

//==========================================================
static const BleCharacteristicDescriptor battery_service_characteristics[] = {
    {
        .intercom_index = BleSrvBatteryCharacterIndexBatteryLevel,
        .name = "Battery Level",
        .data_size = sizeof(uint8_t),
#if defined(SI917)
        .uuid = {.Char_UUID_16 = 0x2A19},
        .uuid_size = 2,
#endif
        // .char_properties = RSI_BLE_ATT_PROPERTY_READ | RSI_BLE_ATT_PROPERTY_NOTIFY,
    },
    {
        .intercom_index = BleSrvBatteryCharacterIndexBatteryStatus,
        .name = "Battery Status",
        .data_size = sizeof(BatteryStatusInfo),
#if defined(SI917)
        .uuid = {.Char_UUID_16 = 0x2BED},
        .uuid_size = 2,
#endif
        // .char_properties = RSI_BLE_ATT_PROPERTY_READ | RSI_BLE_ATT_PROPERTY_NOTIFY,
    },
};

const BleServiceDescriptor ble_service_config_battery = {
    .name = "Battery Service",
#if defined(SI917)
    .uuid = {.Char_UUID_16 = 0x180F},
    .uuid_size = 2,
#endif
    .index = BleIntercomServiceIndexBattery,
    .init_method = BleServiceInitMethodLocal,
    .char_count = COUNT_OF(battery_service_characteristics),
    .char_descriptors = battery_service_characteristics,
    .init = ble_service_battery_init,
    // .on_response = ble_service_battery_on_response,

};
