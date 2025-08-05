#include "ble_service_uart.h"

#include "../ble_service_i.h"
#include <furi_hal_info.h>
#include <stdint.h>

#define TAG "BleUart"

#define UART_SERVICE_UUID \
    {0x6E, 0x40, 0x00, 0x01, 0xB5, 0xA3, 0xF3, 0x93, 0xE0, 0xA9, 0xE5, 0x0E, 0x24, 0xDC, 0xCA, 0x9E}
#define UART_RX_CHAR_UUID \
    {0x6E, 0x40, 0x00, 0x02, 0xB5, 0xA3, 0xF3, 0x93, 0xE0, 0xA9, 0xE5, 0x0E, 0x24, 0xDC, 0xCA, 0x9E}
#define UART_TX_CHAR_UUID \
    {0x6E, 0x40, 0x00, 0x03, 0xB5, 0xA3, 0xF3, 0x93, 0xE0, 0xA9, 0xE5, 0x0E, 0x24, 0xDC, 0xCA, 0x9E}

typedef enum {
    BleSrvDeviceInfoCharacterIndexSerialNumber,
    BleSrvDeviceInfoCharacterIndexHardwareRevision,
    BleSrvDeviceInfoCharacterIndexSoftwareRevision,
} BleSrvDeviceInfoCharacterIndex;

static bool ble_service_uart_init(void* instance) {
    furi_assert(instance);
    BLE_LOG_W("uart_init");

    return true;
}

static const BleCharacteristicDescriptor uart_service_characteristics[] = {
    {
        .intercom_index = 0,
        .name = "Uart Rx",
        .data_size = sizeof(2),
#if defined(SI917)
        .uuid = {.Char_UUID_128 = UART_RX_CHAR_UUID},
        .uuid_size = 16,
#endif
        // .char_properties = RSI_BLE_ATT_PROPERTY_READ | RSI_BLE_ATT_PROPERTY_WRITE,
    },
    {
        .intercom_index = 1,
        .name = "Uart Tx",
        .data_size = sizeof(2),
#if defined(SI917)
        .uuid = {.Char_UUID_128 = UART_TX_CHAR_UUID},
        .uuid_size = 16,
#endif
        // .char_properties = RSI_BLE_ATT_PROPERTY_READ | RSI_BLE_ATT_PROPERTY_NOTIFY,
    },
};

//==========================================================

const BleServiceDescriptor ble_service_config_uart = {
    .name = "Nordic UART",
#if defined(SI917)
    .uuid = {.Char_UUID_128 = UART_SERVICE_UUID},
    .uuid_size = 16,
#endif
    .index = BleIntercomServiceIndexUart,
    .init_method = BleServiceInitMethodRemote,
    .char_count = COUNT_OF(uart_service_characteristics),
    .char_descriptors = uart_service_characteristics,
    .init = ble_service_uart_init,
    // .run = ble_service_device_info_run,
    // .on_response = ble_service_device_info_on_response,
};
