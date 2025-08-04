#pragma once

#include "../ble_common.h"
#include "ble_service_config_types.h"

#include <furi.h>

typedef struct BleServiceObject BleServiceObject;

typedef void (
    *BleServiceStateChangeCallback)(BleServiceObject* instance, BleServiceState new_state);

BleServiceObject* ble_service_alloc(
    const BleServiceDescriptor* service_config,
    FuriMessageQueue* dest_queue,
    Intercom* intercom);

bool ble_service_run(BleServiceObject* instance, const BleMessage* msg);
void ble_service_process_mailbox(BleServiceObject* instance, BleIntercomFrameGeneric* input_frame);
void ble_service_eqnueue_init(BleServiceObject* instance);
