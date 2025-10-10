#pragma once

#include <furi.h>

typedef enum {
    BleIntercomFrameSourceSystem,
    BleIntercomFrameSourceService,
} BleIntercomFrameSource;

typedef enum {
    BleIntercomFrameTypeRequest,
    BleIntercomFrameTypeResponse,
} BleIntercomFrameType;

typedef uint8_t BleCommandCode;

typedef struct {
    BleCommandCode command;
    uint16_t index;
    // uint16_t char_index;
} BleServiceCommand;

typedef struct /*FURI_PACKED*/ {
    BleIntercomFrameSource source;
    BleIntercomFrameType frame_type;
    union {
        BleCommandCode system;
        BleServiceCommand service;
    } command;
    size_t data_size;
} BleIntercomFrameHeader;

#define MAX_BLE_INTERCOM_FRAME_SIZE (512U - sizeof(BleIntercomFrameHeader))

typedef struct {
    BleIntercomFrameHeader header;
    uint8_t data[MAX_BLE_INTERCOM_FRAME_SIZE];
} BleIntercomFrameGeneric;

//==========================================================================================================

typedef struct {
    uint8_t index;
    uint8_t data_size;
} BleCharacteristicDataHeader;

typedef struct /*FURI_PACKED*/ {
    BleCharacteristicDataHeader header;
    uint8_t data[];
} BleCharacteristicData;

typedef uint8_t BleCharacteristicCountType;

typedef struct {
    BleCharacteristicCountType char_count;
    BleCharacteristicData chars_config[];
} BleIntercomServiceData;

typedef struct /*FURI_PACKED*/ {
    BleIntercomFrameHeader header;
    BleIntercomServiceData service_init;
} BleIntercomFrameServiceConfig;

//=============================================
