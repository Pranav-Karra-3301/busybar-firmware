#pragma once

#include "../ble_service_i.h"

bool ble_service_target_init(BleServiceObject* instance);

// typedef bool (*BleServiceProcessRequestFrameByStateCallback)();

// static bool dummy() {
// }

// static const BleServiceProcessRequestFrameByStateCallback request_handlers[] = {
//     [BleServiceStateReset] = dummy,
//     [BleServiceStateInitialization] = ble_service_target_init,
//     [BleServiceStateReady] = dummy,
//     [BleServiceStateAdvertising] = dummy,
//     [BleServiceStateConnected] = dummy,
//     [BleServiceStateError] = dummy,
// };
