#pragma once

#include "ble_service_config_types.h"

typedef struct BleCharacteristicObject BleCharacteristicObject;

BleCharacteristicObject* ble_characteristic_alloc(const BleCharacteristicDescriptor* config);
void ble_characteristic_free(BleCharacteristicObject* instance);

void* ble_characteristic_get_data(BleCharacteristicObject* instance);
size_t ble_characteristic_get_data_size(BleCharacteristicObject* instance);
void ble_characteristic_set_data(
    BleCharacteristicObject* instance,
    const void* data,
    const size_t data_size);
bool ble_characteristic_modified(BleCharacteristicObject* instance);

const BleCharacteristicDescriptor*
    ble_characteristic_get_config(BleCharacteristicObject* instance);

uint8_t ble_characteristic_fill_update_struct(
    BleCharacteristicObject* instance,
    BleCharacteristicData* output);
