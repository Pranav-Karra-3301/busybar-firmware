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

inline void ble_service_set_state(BleServiceObject* instance, BleServiceState new_state) {
    //service_lock()
    ble_service_enqueue_message(
        instance, BleCommandServiceSetState, &new_state, sizeof(BleServiceState));
    //service_unlock();
}
