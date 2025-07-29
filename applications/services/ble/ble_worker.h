#pragma once

void ble_worker_init();

void ble_worker_init_service(BleIntercomFrameServiceConfig* config);

void ble_worker_set_value(
    uint16_t service_index,
    uint16_t char_index,
    uint16_t data_size,
    uint8_t* data);

void ble_worker_start();

void ble_worker_stop();
