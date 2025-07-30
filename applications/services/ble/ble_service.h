#pragma once

#include "ble_common.h"

#include <intercom/intercom.h>

extern const BleServiceDescriptor service_config[];

BleServiceObject* ble_worker_create_service(
    const BleServiceDescriptor* service_config,
    FuriSemaphore* access,
    Intercom* intercom);

bool ble_service_common_init(void* context, BleIntercomFrameGeneric* frame);
void ble_service_common_write(
    void* context,
    uint16_t char_index,
    const void* data,
    size_t data_size,
    BleIntercomFrameGeneric* frame);