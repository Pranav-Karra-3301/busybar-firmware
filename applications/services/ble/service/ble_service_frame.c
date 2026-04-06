#include "ble_service_frame.h"
#include "../ble_intercom_types.h"

/**
 * @brief This must be less than @ref INTERCOM_SYNC_CHAR_TIMEOUT_MS.
 */
#define BLE_SERVICE_INPUT_FRAME_LOCK_TIMEOUT (500)

struct BleServiceFrame {
    bool pending;
    FuriSemaphore* lock;
    size_t size;
    uint8_t* buf;
};

BleServiceFrame* ble_service_frame_alloc() {
    BleServiceFrame* instance = malloc(sizeof(BleServiceFrame));
    instance->lock = furi_semaphore_alloc(1, 1);
    instance->size = 0;
    return instance;
}

void ble_service_frame_free(BleServiceFrame* instance) {
    furi_assert(instance);
    furi_semaphore_free(instance->lock);
    free(instance);
}

bool ble_service_frame_pending(BleServiceFrame* instance) {
    furi_assert(instance);
    return instance->pending;
}

bool ble_service_frame_put_data(BleServiceFrame* instance, const void* data, size_t size) {
    furi_assert(instance);
    furi_assert(data);
    bool result = false;
    if(ble_service_frame_lock(instance)) {
        ble_service_frame_check_resize(instance, size);
        memcpy(instance->buf, data, size);
        result = true;
    }
    return result;
}

void* ble_service_frame_get_data_ptr(BleServiceFrame* instance) {
    furi_assert(instance);
    return instance->buf;
}

bool ble_service_frame_lock(BleServiceFrame* instance) {
    furi_assert(instance);
    instance->pending = furi_semaphore_acquire(
                            instance->lock, BLE_SERVICE_INPUT_FRAME_LOCK_TIMEOUT) == FuriStatusOk;
    // if(furi_semaphore_acquire(instance->lock, BLE_SERVICE_INPUT_FRAME_LOCK_TIMEOUT) !=
    //    FuriStatusOk) {
    //     return false;
    // }
    // instance->pending = true;
    // return true;
    return instance->pending;
}

void ble_service_frame_unlock(BleServiceFrame* instance) {
    furi_assert(instance);
    memset(instance->buf, 0, instance->size);
    instance->pending = false;
    if(furi_semaphore_release(instance->lock) != FuriStatusOk) {
        // BLE_LOG_W("%s - frame unlock failed", instance->config->name);
    }
}

void ble_service_frame_check_resize(BleServiceFrame* instance, size_t new_frame_size) {
    furi_assert(instance);
    furi_assert(new_frame_size < MAX_BLE_INTERCOM_FRAME_SIZE);

    if(new_frame_size > instance->size) {
        instance->buf = realloc(instance->buf, new_frame_size);
        instance->size = new_frame_size;
        // BLE_LOG_D("%s - buf_size: %d", instance->config->name, new_frame_size);
    }
}
