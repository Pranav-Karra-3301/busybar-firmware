#pragma once

#include <furi.h>

///TODO: here we will place all includes for services, but for now we will place everything here

typedef struct {
    uint8_t flags;
    uint16_t power_state;
    uint8_t battery_level;
} FURI_PACKED BatteryStatusInfo;

//=============================================
typedef enum {
    BleIntercomCharIndexBatteryLevel,
    BleIntercomCharIndexBatteryStatus,

    BleIntercomCharIndexUartRx,
    BleIntercomCharIndexUartTx,

    BleIntercomCharIndexDeviceInfoSerialNumber,
    BleIntercomCharIndexDeviceInfoHardwareRevision,
    BleIntercomCharIndexDeviceInfoSoftwareRevision,
} BleIntercomCharIndex;

//=============================================

typedef enum {
    BleIntercomRequestTypeRead,
    BleIntercomRequestTypeWrite,
    BleIntercomRequestTypeNotify,
} BleIntercomRequestType;

typedef struct {
    BleIntercomRequestType type;
    BleIntercomCharIndex char_index;
    size_t data_size;
    uint8_t data[];
} BleIntercomRequest;
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
    BleIntercomCharIndex intercom_index;
    Char_UUID_t uuid;
    uint8_t uuid_size;
    uint8_t data_size;
    uint8_t char_properties;
    uint8_t security_permissions;
    const char* name;

    ///TODO:This might be optional and everything could be done via same handlers
    //If NULL invoke default handler
    const BleCharacteristicOnRead on_read;
    const BleCharacteristicOnWrite on_write;
    const BleCharacteristicOnNotify on_notify;
} BleCharacteristicDescriptor;

typedef struct {
    Char_UUID_t uuid;
    uint8_t uuid_size;
    uint8_t char_count;
    const BleCharacteristicDescriptor* const char_descriptors;
    const char* name;
} BleServiceDescriptor;

typedef struct {
    const BleCharacteristicDescriptor* desc;
    uint16_t handle;
    void* data;
} BleCharacteristicObject;
