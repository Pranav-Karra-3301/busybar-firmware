#pragma once

#include "ble_service.h"

typedef struct {
    const BleCharacteristicDescriptor* desc;
    uint16_t handle;
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
    uint8_t frame_buf[100];
    //uint8_t* frame_buf;
    BleServiceStateChangeCallback state_change_callback;
#if defined(SI917)
    void* service_handler;
#endif
};

void ble_service_enqueue_message(
    BleServiceObject* instance,
    BleCommand command,
    void* data,
    uint8_t data_size);
