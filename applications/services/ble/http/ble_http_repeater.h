#pragma once

#include "../ble.h"

typedef struct BleHttpRepeater BleHttpRepeater;

BleHttpRepeater* ble_http_repeater_alloc(Ble* ble);
void ble_http_repeater_free(BleHttpRepeater* instance);
void ble_http_repeater_update(BleHttpRepeater* instance, const BleServiceStatus status);
