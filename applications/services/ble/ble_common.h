#pragma once

#include <furi.h>

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
} /*FURI_PACKED*/ BatteryStatusInfo;

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
    BleIntercomServiceIndexBattery,
    BleIntercomServiceIndexDeviceInfo,
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

typedef struct {
    BleIntercomFrameHeader header;
    // size_t data_size;
    //TODO: this can be moved to the data below, so we will send characteristic data only when needed;
    uint16_t char_index;
    uint8_t data[];
} BleIntercomFrameCharData;

typedef struct /*FURI_PACKED*/ {
    uint8_t intercom_index;
    uint8_t data_size;
} BleCharSize;

typedef struct /*FURI_PACKED*/ {
    BleIntercomFrameHeader header;
    uint8_t char_count;
    BleCharSize chars_config[];
} BleIntercomFrameServiceConfig;

//=============================================

typedef union {
    /**
   * 16-bit UUID
   */
    uint16_t Char_UUID_16;
    /**
   * 128-bit UUID
   */
    uint8_t Char_UUID_128[16];
} Char_UUID_t;

// typedef void* (*BleCharacteristicAlloc)(const BleCharacteristicDescriptor* desc);
typedef void (*BleCharacteristicOnRead)(void* data, uint8_t data_size);
typedef void (*BleCharacteristicOnWrite)(void* data, uint8_t data_size);
typedef void (*BleCharacteristicOnNotify)(void* data, uint8_t data_size);

typedef struct {
    uint16_t intercom_index;
    Char_UUID_t uuid;
    uint8_t uuid_size;
    uint8_t data_size;
    uint8_t char_properties;
    uint8_t security_permissions;
    const char* name;

    ///TODO:This might be optional and everything could be done via same handlers
    //If NULL invoke default handler
    // const BleCharacteristicOnRead on_read;
    // const BleCharacteristicOnWrite on_write;
    // const BleCharacteristicOnNotify on_notify;
} BleCharacteristicDescriptor;

typedef enum {
    BleServiceInitMethodLocal,
    BleServiceInitMethodRemote,
} BleServiceInitMethod;

typedef struct {
    BleIntercomServiceIndex index;
    Char_UUID_t uuid;
    uint8_t uuid_size;
    uint8_t char_count;
    BleServiceInitMethod init_method;
    const BleCharacteristicDescriptor* char_descriptors;
    const char* name;
} BleServiceDescriptor;

typedef struct {
    const BleCharacteristicDescriptor* desc;
    uint16_t handle;
    void* data;
} BleCharacteristicObject;

typedef enum {
    BleServiceStateIdle,
    BleServiceStateReady,
} BleServiceState;

typedef struct {
    // BleIntercomServiceIndex intercom_index;
    BleServiceState state;
    const BleServiceDescriptor* desc;
    BleCharacteristicObject** chars;

    // #if defined(TARGET_F64)
    void* service_handler;
    // #endif
} BleServiceObject;
