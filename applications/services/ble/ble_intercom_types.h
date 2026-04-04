#pragma once

#include <intercom/intercom.h>
#include <intercom/intercom_frame.h>
#include <furi.h>

typedef enum {
    BleIntercomFrameSourceUnknown,
    BleIntercomFrameSourceSystem,
    BleIntercomFrameSourceService,
} BleIntercomFrameSource;

typedef enum {
    BleIntercomFrameTypeUnknown,
    BleIntercomFrameTypeRequest,
    BleIntercomFrameTypeResponse,
} BleIntercomFrameType;

typedef uint8_t BleCommandCode;

typedef struct {
    bool result;
    BleCommandCode command;
    uint16_t service_index;

    uint32_t num;
    BleIntercomFrameSource source;
    BleIntercomFrameType frame_type;
    size_t data_size;
} BleIntercomFrameHeader;

#define MAX_BLE_INTERCOM_FRAME_SIZE (INTERCOM_FRAME_DATA_SIZE - sizeof(BleIntercomFrameHeader))

typedef struct {
    BleIntercomFrameHeader header;
    uint8_t data[MAX_BLE_INTERCOM_FRAME_SIZE];
} BleIntercomFrameGeneric;

//==========================================================================================================

typedef enum {
    BleCharacteristicFrameTypeUnknown,
    BleCharacteristicFrameTypeRequest,
    BleCharacteristicFrameTypeResponse,
} BleCharacteristicFrameType;

typedef struct {
    uint8_t index;
    uint16_t data_size;
    BleCharacteristicFrameType frame_type;
    uint32_t seq_num;
} BleCharacteristicDataHeader;

typedef struct {
    BleCharacteristicDataHeader header;
    uint8_t data[];
} BleCharacteristicData;

typedef size_t BleCharacteristicCountType;

typedef struct {
    BleCharacteristicCountType char_count;
    BleCharacteristicData chars_config[];
} BleIntercomServiceData;

typedef struct {
    BleIntercomFrameHeader header;
    BleIntercomServiceData service_init;
} BleIntercomFrameServiceConfig;

//=============================================
