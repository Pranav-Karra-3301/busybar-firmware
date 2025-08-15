#pragma once

#include "ble_state.h"
#include <furi.h>

/**
 * @brief BLE FURI record identifier.
 */
#define RECORD_BLE "ble"

#define BLE_AUTO_INIT

typedef struct Ble Ble;

bool ble_init(Ble* ble);

BleServiceState ble_get_state(Ble* ble);

bool ble_start(Ble* ble);

bool ble_stop(Ble* ble);
