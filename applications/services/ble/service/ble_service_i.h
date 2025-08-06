#pragma once

#include "ble_service.h"
#include <furi.h>

#define BLE_ATT_PROPERTY_READ   0x02
#define BLE_ATT_PROPERTY_WRITE  0x08
#define BLE_ATT_PROPERTY_NOTIFY 0x10

typedef struct {
    const BleCharacteristicDescriptor* desc;
    uint16_t handle;
    uint8_t data_size;
    void* data;
} BleCharacteristicObject;

struct BleServiceObject {
    BleServiceState state;
    const BleServiceDescriptor* desc;
    BleCharacteristicObject** chars;

    FuriMessageQueue* message_queue;
    FuriMutex* service_lock;
    Intercom* intercom;

    FuriSemaphore* frame_lock;
    size_t frame_size;
    ///TODO: replace this with malloc
    uint8_t frame_buf[50];
    //uint8_t* frame_buf;

    uint8_t output[50];
    BleServiceStateChangeCallback state_change_callback;
    void* data_context;
#if defined(SI917)
    void* service_handler;
#endif
};

void ble_service_enqueue_message(
    BleServiceObject* instance,
    BleCommand command,
    void* data,
    uint8_t data_size);

void ble_service_prepare_frame(
    BleServiceObject* instance,
    BleIntercomFrameType frame_type,
    uint8_t command_event,
    size_t data_size,
    void* data);

void ble_service_switch_state(
    BleServiceObject* instance,
    BleServiceState new_state/* ,
    bool notify_remote */);
