#pragma once

#include <furi.h>

#include <intercom/intercom.h>

#define BLE_DEBUG

#ifdef BLE_DEBUG
#define BLE_LOG_D(...) FURI_LOG_D(TAG, __VA_ARGS__)
#define BLE_LOG_I(...) FURI_LOG_I(TAG, __VA_ARGS__)
#define BLE_LOG_W(...) FURI_LOG_W(TAG, __VA_ARGS__)
#else
#define BLE_LOG_D(...)
#define BLE_LOG_I(...)
#define BLE_LOG_W(...)
#endif

typedef enum {
    BleIntercomFrameTypeRequest,
    BleIntercomFrameTypeResponse,
    BleIntercomFrameTypeNotification,
    BleIntercomFrameTypeHeartbeat,
} BleIntercomFrameType;

typedef enum {
    BleServiceStateReset, /*Service was just created. Will move to BleServiceStateInitialization when it will create all inner objects*/
    BleServiceStateInitialization, /* Service performs initialization sequence for all inner ble services. 
    U5 also sends init data to 917 to help him create its services */
    BleServiceStateReady, /*All init sequences are done. All inner services configured, and both u5 and 917 ready to work. But ble still disabled*/
    BleServiceStateAdvertising, /*User enabled ble, device start advertising.*/
    BleServiceStateConnected, /*Remote device connected to bsb over ble*/
    BleServiceStateError, /*Error occured.*/
} BleServiceState;
//==========================================================================================================
//Test shit
///TODO: rename this to BleCommandType
typedef enum {
    //keep these
    BleCommandEnable,
    BleCommandDisable,
    //-------------------------------------
    BleCommandServiceInit,

    BleCommandServiceRead,
    BleCommandServiceWrite,
    BleCommandServiceNotify,

    //-------------------------------------

    BleCommandServiceProcessFrame,
} BleCommand;

// typedef enum {
//     BleEventStateChanged,
// } BleEvent;

// typedef union {
//     BleCommand command;
//     BleEvent event;
// } BleCommandEvent;

///TODO: need to exted this more
typedef struct {
    BleCommand type; ///TODO: get rid if this
    uint16_t service_index;
    uint8_t data[5];
    // FuriApiLock lock;
    bool result; ///TODO: replace with some more extended status
} BleMessage;
//==========================================================================================================

typedef enum {
    BleIntercomServiceIndexDeviceInfo,
    BleIntercomServiceIndexBattery,
    BleIntercomServiceIndexUart,
} BleIntercomServiceIndex;
//=============================================

typedef struct /*FURI_PACKED*/ {
    BleIntercomFrameType frame_type;
    BleCommand command;
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
    BleServiceState state;
} BleIntercomFrameHeartbeat;

typedef struct /*FURI_PACKED*/ {
    BleIntercomFrameHeader header;
    uint16_t char_index;
    uint8_t data[];
} BleIntercomFrameCharData;

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
} BleServiceInitConfig;

typedef struct /*FURI_PACKED*/ {
    BleIntercomFrameHeader header;
    BleServiceInitConfig service_init;
} BleIntercomFrameServiceConfig;

//=============================================
