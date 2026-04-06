#pragma once
#include <furi.h>

typedef struct BleServiceFrame BleServiceFrame;

BleServiceFrame* ble_service_frame_alloc(void);
void ble_service_frame_free(BleServiceFrame* instance);
bool ble_service_frame_pending(BleServiceFrame* instance);
bool ble_service_frame_put_data(BleServiceFrame* instance, const void* data, size_t size);
void* ble_service_frame_get_data_ptr(BleServiceFrame* instance);

bool ble_service_frame_lock(BleServiceFrame* instance);
void ble_service_frame_unlock(BleServiceFrame* instance);
void ble_service_frame_check_resize(BleServiceFrame* instance, size_t new_frame_size);
