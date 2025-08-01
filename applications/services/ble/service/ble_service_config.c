#include "ble_service_config.h"
#include "device_info/ble_service_device_info.h"
#include "battery/ble_service_battery.h"
#include "state/ble_service_state.h"

const BleServiceDescriptor* service_config[] = {
    [BleIntercomServiceIndexDeviceInfo] = &ble_service_config_device_info,
    [BleIntercomServiceIndexBattery] = &ble_service_config_battery,
    [BleIntercomServiceIndexState] = &ble_service_state,
};
// static const BleCharacteristicDescriptor battery_service_characteristics[] = {
//     {
//         .intercom_index = BleSrvBatteryCharacterIndexBatteryLevel,
//         .name = "Battery Level",
//         .uuid = {.Char_UUID_16 = 0x2A19},
//         .uuid_size = 2,
//         .data_size = sizeof(uint8_t),
//         // .char_properties = RSI_BLE_ATT_PROPERTY_READ | RSI_BLE_ATT_PROPERTY_NOTIFY,
//     },
//     {
//         .intercom_index = BleSrvBatteryCharacterIndexBatteryStatus,
//         .name = "Battery Status",
//         .uuid = {.Char_UUID_16 = 0x2BED},
//         .uuid_size = 2,
//         .data_size = sizeof(BatteryStatusInfo),
//         // .char_properties = RSI_BLE_ATT_PROPERTY_READ | RSI_BLE_ATT_PROPERTY_NOTIFY,
//     },
// };

// //==========================================================
/* #define UART_SERVICE_UUID \
    {0x6E, 0x40, 0x00, 0x01, 0xB5, 0xA3, 0xF3, 0x93, 0xE0, 0xA9, 0xE5, 0x0E, 0x24, 0xDC, 0xCA, 0x9E}
#define UART_RX_CHAR_UUID \
    {0x6E, 0x40, 0x00, 0x02, 0xB5, 0xA3, 0xF3, 0x93, 0xE0, 0xA9, 0xE5, 0x0E, 0x24, 0xDC, 0xCA, 0x9E}
#define UART_TX_CHAR_UUID \
    {0x6E, 0x40, 0x00, 0x03, 0xB5, 0xA3, 0xF3, 0x93, 0xE0, 0xA9, 0xE5, 0x0E, 0x24, 0xDC, 0xCA, 0x9E} */

// static const BleCharacteristicDescriptor nordic_uart_service_characteristics[] = {
//     {
//         .intercom_index = 0,
//         .name = "Uart Rx",
//         .uuid = {.Char_UUID_128 = UART_RX_CHAR_UUID},
//         .uuid_size = 16,
//         .data_size = sizeof(2),
//         // .char_properties = RSI_BLE_ATT_PROPERTY_READ | RSI_BLE_ATT_PROPERTY_WRITE,
//     },
//     {
//         .intercom_index = 1,
//         .name = "Uart Tx",
//         .uuid = {.Char_UUID_128 = UART_TX_CHAR_UUID},
//         .uuid_size = 16,
//         .data_size = sizeof(2),
//         // .char_properties = RSI_BLE_ATT_PROPERTY_READ | RSI_BLE_ATT_PROPERTY_NOTIFY,
//     },
// };

// //==========================================================
// const BleServiceDescriptor service_config[] = {
//     [BleIntercomServiceIndexDeviceInfo] =
//         {
//             .name = "Device Information",
//             .uuid = {.Char_UUID_16 = 0x180A},
//             .uuid_size = 2,
//             .index = BleIntercomServiceIndexDeviceInfo,
//             .init_method = BleServiceInitMethodRemote,
//             .char_count = COUNT_OF(device_info_service_characteristics),
//             .char_descriptors = device_info_service_characteristics,
//             .init = ble_service_device_info_init,
//             .run = ble_service_device_info_run,
//             // .on_response = ble_service_device_info_on_response,
//         },
//     [BleIntercomServiceIndexBattery] =
//         {
//             .name = "Battery Service",
//             .uuid = {.Char_UUID_16 = 0x180F},
//             .uuid_size = 2,
//             .index = BleIntercomServiceIndexBattery,
//             .init_method = BleServiceInitMethodLocal,
//             .char_count = COUNT_OF(battery_service_characteristics),
//             .char_descriptors = battery_service_characteristics,
//             .init = ble_service_battery_init,
//             // .on_response = ble_service_battery_on_response,
//         },
//     [BleIntercomServiceIndexUart] =
//         {
//             .name = "Nordic UART",
//             .uuid = {.Char_UUID_128 = UART_SERVICE_UUID},
//             .uuid_size = 16,
//             .index = BleIntercomServiceIndexUart,
//             .init_method = BleServiceInitMethodLocal,
//             .char_count = COUNT_OF(nordic_uart_service_characteristics),
//             .char_descriptors = nordic_uart_service_characteristics,
//         },
// };

// BleCharacteristicObject* ble_characteristic_alloc(const BleCharacteristicDescriptor* desc) {
//     // furi_assert(desc);
//     // furi_assert(service_handler);
//     // furi_assert(desc->data_size);
//     // furi_assert(out_handle);

//     // uuid_t uuid = {0};
//     // ble_prepare_uuid(&desc->uuid, desc->uuid_size, &uuid);

//     BleCharacteristicObject* instance = malloc(sizeof(BleCharacteristicObject));
//     instance->desc = desc;

//     return instance;
// }

// #ifndef(SI917)
