
#include "ble_characteristic.h"
#include <furi.h>

struct BleCharacteristicObject {
    uint16_t handle;
    bool modified;
    uint8_t data_size; ///TODO: set data_type of proper size
    void* data;
    const BleCharacteristicDescriptor* descriptor;
};

BleCharacteristicObject* ble_characteristic_alloc(const BleCharacteristicDescriptor* config) {
    furi_assert(config);
    BleCharacteristicObject* instance = malloc(sizeof(BleCharacteristicObject));
    instance->descriptor = config;
    return instance;
}

void ble_characteristic_free(BleCharacteristicObject* instance) {
    furi_assert(instance);
    if(instance->data) free(instance->data);
    free(instance);
}

const void* ble_characteristic_get_data(BleCharacteristicObject* instance) {
    furi_assert(instance);
    return instance->data;
}

size_t ble_characteristic_get_data_size(BleCharacteristicObject* instance) {
    furi_assert(instance);
    return instance->data_size;
}

void ble_characteristic_set_data(
    BleCharacteristicObject* instance,
    const void* data,
    const size_t data_size) {
    furi_assert(instance);
    furi_assert(data);
    furi_assert(data_size > 0);

    if(instance->data == NULL) {
        instance->data = malloc(data_size);
        instance->data_size = data_size;
    }

    furi_assert(instance->data_size >= data_size);
    memcpy(instance->data, data, data_size);
    instance->data_size = data_size;
    instance->modified = true;
}

bool ble_characteristic_modified(BleCharacteristicObject* instance) {
    furi_assert(instance);
    return instance->modified;
}

const BleCharacteristicDescriptor*
    ble_characteristic_get_config(BleCharacteristicObject* instance) {
    furi_assert(instance);
    return instance->descriptor;
}

uint8_t ble_characteristic_fill_update_struct(
    BleCharacteristicObject* instance,
    BleCharacteristicData* output) {
    furi_assert(instance);
    furi_assert(output);

    output->header.index = instance->descriptor->intercom_index;
    output->header.data_size = instance->data_size;
    FURI_LOG_D(instance->descriptor->name, "Char size: %d", instance->data_size);

    memcpy(output->data, instance->data, instance->data_size);
    instance->modified = false;
    return (instance->data_size + sizeof(BleCharacteristicDataHeader));
}
