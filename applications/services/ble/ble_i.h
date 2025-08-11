#pragma once

#include "ble.h"
#include "ble_common.h"
#include "service/ble_service.h"
#include "service/ble_service_config.h"

#include <intercom/intercom.h>
#include <furi.h>

struct Ble {
    BleServiceState state;

    FuriMessageQueue* message_queue;

    FuriSemaphore* mailbox_lock;
    BleIntercomFrameGeneric mailbox;

    FuriEventLoopTimer* init_timer;
    // FuriEventLoopTimer* test_timer;
    FuriMutex* ble_lock;

    FuriEventLoop* event_loop;
    Intercom* intercom;
    //--------------------------

    FuriSemaphore* access_semaphore;

    BleServiceObject* services[BLE_SERVICES_COUNT];
    BleMessage* current_message;
};
