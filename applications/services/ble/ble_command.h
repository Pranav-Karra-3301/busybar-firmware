#pragma once

#include "ble_i.h"

void ble_command_handler_enable(void);

void ble_command_handler_disable(void);

void ble_command_handler_get_status(Ble* instance, BleIntercomFrameStatus* frame);
