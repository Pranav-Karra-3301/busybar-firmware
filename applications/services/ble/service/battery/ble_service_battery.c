#include "ble_service_battery.h"

#include "../ble_service_i.h"

#define TAG "BleBattery"

typedef enum {
    BleSrvBatteryCharacterIndexBatteryLevel,
    BleSrvBatteryCharacterIndexBatteryStatus,
} BleSrvBatteryCharacterIndex;

typedef union FURI_PACKED {
    struct FURI_PACKED {
        uint8_t battery_present         : 1;
        uint8_t wired_source_present    : 2;
        uint8_t wireless_source_present : 2;
        uint8_t battery_charge_state    : 2;
        uint8_t battery_charge_level    : 2;
        uint8_t charging_type           : 3;
        uint8_t charging_fault_reason   : 3;
        uint8_t rfu                     : 1;
    } fields;
    uint16_t value;
} BatteryPowerState;

typedef struct FURI_PACKED {
    uint8_t flags;
    BatteryPowerState state;
} BatteryStatusInfo;

static bool ble_service_battery_init(void* object) {
    furi_assert(object);

    BLE_LOG_W("battery_init");

    BleServiceObject* instance = object;

    uint8_t* battery_level = malloc(sizeof(uint8_t));
    *battery_level = 56;
    instance->chars[BleSrvBatteryCharacterIndexBatteryLevel]->data = battery_level;
    instance->chars[BleSrvBatteryCharacterIndexBatteryLevel]->data_size = sizeof(uint8_t);

    BatteryStatusInfo* battery_status = malloc(sizeof(BatteryStatusInfo));

    battery_status->flags = 0;
    battery_status->state.fields.battery_present = 1;
    battery_status->state.fields.wired_source_present = 1;
    battery_status->state.fields.wireless_source_present = 0;
    battery_status->state.fields.battery_charge_state = 2;
    battery_status->state.fields.battery_charge_level = 1;
    battery_status->state.fields.charging_type = 2;
    battery_status->state.fields.charging_fault_reason = 0;

    instance->chars[BleSrvBatteryCharacterIndexBatteryStatus]->data = battery_status;
    instance->chars[BleSrvBatteryCharacterIndexBatteryStatus]->data_size =
        sizeof(BatteryStatusInfo);

    return true;
}

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
    .index = BleIntercomServiceIndexBattery,
    .init_method = BleServiceInitMethodLocal,
    .char_count = COUNT_OF(battery_service_characteristics),
    .char_descriptors = battery_service_characteristics,
    .init = ble_service_battery_init,
    // .on_response = ble_service_battery_on_response,

};
