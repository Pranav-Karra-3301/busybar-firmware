#pragma once

#include <furi.h>

#include <intercom/intercom.h>

#define BLE_DEBUG

#ifdef BLE_DEBUG
#define BLE_LOG_I(...) FURI_LOG_I(TAG, __VA_ARGS__)
#define BLE_LOG_W(...) FURI_LOG_W(TAG, __VA_ARGS__)
#else
#define BLE_LOG_D(...)
#define BLE_LOG_W(...)
#endif

///TODO: here we will place all includes for services, but for now we will place everything here

typedef struct {
    uint8_t flags;
    uint16_t power_state;
    uint8_t battery_level;
} FURI_PACKED BatteryStatusInfo;

//=============================================
// typedef enum {
//     BleIntercomCharIndexBatteryLevel,
//     BleIntercomCharIndexBatteryStatus,

//     BleIntercomCharIndexUartRx,
//     BleIntercomCharIndexUartTx,

//     BleIntercomCharIndexDeviceInfoSerialNumber,
//     BleIntercomCharIndexDeviceInfoHardwareRevision,
//     BleIntercomCharIndexDeviceInfoSoftwareRevision,
// } BleIntercomCharIndex;

typedef enum {
    BleSrvDeviceInfoCharacterIndexSerialNumber,
    BleSrvDeviceInfoCharacterIndexHardwareRevision,
    BleSrvDeviceInfoCharacterIndexSoftwareRevision,
} BleSrvDeviceInfoCharacterIndex;

typedef enum {
    BleIntercomServiceIndexDeviceInfo,
    BleIntercomServiceIndexBattery,
    BleIntercomServiceIndexUart,
} BleIntercomServiceIndex;
//=============================================

typedef enum {
    BleRequestTypeEnable,
    BleRequestTypeDisable,
    BleRequestTypeInit,
    BleRequestTypeRead,
    BleRequestTypeWrite,
    BleRequestTypeNotify,
} BleRequestType;

typedef struct /*FURI_PACKED*/ {
    BleRequestType type;
    BleIntercomServiceIndex service_index;
    size_t data_size;
} BleIntercomFrameHeader;

#define MAX_BLE_INTERCOM_FRAME_SIZE (512U - sizeof(BleIntercomFrameHeader))

typedef struct {
    BleIntercomFrameHeader header;
    uint8_t data[MAX_BLE_INTERCOM_FRAME_SIZE];
} BleIntercomFrameGeneric;

typedef struct /*FURI_PACKED*/ {
    BleIntercomFrameHeader header;
    // size_t data_size;
    //TODO: this can be moved to the data below, so we will send characteristic data only when needed;
    uint16_t char_index;
    uint8_t data[];
} BleIntercomFrameCharData;

// typedef struct /*FURI_PACKED*/ {
//     uint8_t intercom_index;
//     uint8_t data_size;
// } BleCharSize;

typedef struct /*FURI_PACKED*/ {
    uint8_t intercom_index;
    uint8_t data_size;
    uint8_t data[];
} BleCharSize;

typedef struct /*FURI_PACKED*/ {
    BleIntercomFrameHeader header;
    uint8_t char_count;
    BleCharSize chars_config[];
} BleIntercomFrameServiceConfig;

//=============================================
