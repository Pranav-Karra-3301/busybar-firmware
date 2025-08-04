#include "ble_service_i.h"

#define TAG "BleServiceBase"

static BleCharacteristicObject* ble_characteristic_alloc(const BleCharacteristicDescriptor* desc) {
    BleCharacteristicObject* instance = malloc(sizeof(BleCharacteristicObject));
    instance->desc = desc;

    //target_specific_alloc();
    // {
    // furi_assert(desc);
    // furi_assert(service_handler);
    // furi_assert(desc->data_size);
    // furi_assert(out_handle);

    // uuid_t uuid = {0};
    // ble_prepare_uuid(&desc->uuid, desc->uuid_size, &uuid);
    // }

    return instance;
}

/* inline  */ static bool ble_service_lock(BleServiceObject* instance) {
    if(furi_mutex_acquire(instance->service_lock, 100) != FuriStatusOk) {
        FURI_LOG_W(instance->desc->name, "Service lock failed");
        return false;
    }
    return true;
}

/* inline  */ static void ble_service_unlock(BleServiceObject* instance) {
    if(furi_mutex_release(instance->service_lock) != FuriStatusOk) {
        FURI_LOG_W(instance->desc->name, "Service unlock failed");
    }
}

static bool ble_service_lock_frame(BleServiceObject* instance) {
    if(furi_semaphore_acquire(instance->frame_lock, 100) != FuriStatusOk) {
        FURI_LOG_W(instance->desc->name, "Frame lock failed");
        return false;
    }
    return true;
}

static void ble_service_unlock_frame(BleServiceObject* instance) {
    if(furi_semaphore_release(instance->frame_lock) != FuriStatusOk) {
        FURI_LOG_W(instance->desc->name, "Frame unlock failed");
    }
}

static void ble_service_send_intercom_frame(
    BleServiceObject* instance,
    BleIntercomFrameType frame_type,
    BleCommandEvent cmd_evt,
    size_t data_size,
    void* data) {
    BleIntercomFrameGeneric* frame = (BleIntercomFrameGeneric*)instance->frame_buf;
    frame->header.frame_type = frame_type;
    frame->header.type = cmd_evt;
    frame->header.service_index = instance->desc->index;
    frame->header.data_size = data_size;
    memcpy(frame->data, data, data_size);

    size_t frame_size = data_size + sizeof(BleIntercomFrameHeader);

    FURI_LOG_D(
        instance->desc->name,
        "Tx Frame t: %d c: %d ds: %d fs: %d",
        frame_type,
        cmd_evt.command,
        data_size,
        frame_size);

    size_t tx =
        intercom_tx(instance->intercom, IntercomChannelBle, instance->frame_buf, frame_size, 100);
    furi_assert(tx == frame_size);
}

static bool ble_service_switch_state_allowed(
    const BleServiceState current_state,
    const BleServiceState new_state) {
    UNUSED(current_state);
    UNUSED(new_state);
    return true;
}

static void ble_service_switch_state(BleServiceObject* instance, BleServiceState new_state) {
    if(ble_service_switch_state_allowed(instance->state, new_state)) {
        if(new_state == BleServiceStateInitialization) {
            instance->desc->init(instance);
        }

        BleCommandEvent c = {.event = BleEventStateChanged};
        ble_service_send_intercom_frame(
            instance, BleIntercomFrameTypeNotification, c, sizeof(BleServiceState), &new_state);
    }
}

BleServiceObject* ble_service_alloc(
    const BleServiceDescriptor* service_config,
    FuriMessageQueue* dest_queue,
    Intercom* intercom) {
    furi_assert(service_config);
    furi_assert(dest_queue);

    BleServiceObject* instance = malloc(sizeof(BleServiceObject));
    FURI_LOG_I(service_config->name, "alloc service");

    instance->state = BleServiceStateReset;
    instance->desc = service_config;
    instance->intercom = intercom;
    instance->message_queue = dest_queue;
    instance->frame_lock = furi_semaphore_alloc(1, 1);
    instance->service_lock = furi_mutex_alloc(FuriMutexTypeNormal);

    if(service_config->char_count) {
        instance->chars = malloc(sizeof(BleCharacteristicObject*) * service_config->char_count);
        for(size_t i = 0; i < service_config->char_count; i++) {
            const BleCharacteristicDescriptor* config = &service_config->char_descriptors[i];
            BleCharacteristicObject* ble_char = ble_characteristic_alloc(config);
            instance->chars[config->intercom_index] = ble_char;
        }
    }
    ///TODO: testcode remove after that
    instance->frame_size = sizeof(instance->frame_buf);

    return instance;
}

bool ble_service_run(BleServiceObject* instance, const BleMessage* msg) {
    furi_assert(instance);
    furi_assert(msg);

    FURI_LOG_D(instance->desc->name, "ble_service_run");
    bool result = false;
    if(ble_service_lock(instance)) {
        if(msg->type == BleCommandServiceProcessFrame) {
            FURI_LOG_D(instance->desc->name, "process_input_frame");
        }
        ble_service_unlock(instance);
        result = true;
    }
    return result;
}

void ble_process_mailbox(BleServiceObject* instance, BleIntercomFrameGeneric* input_frame) {
    furi_assert(instance);
    furi_assert(input_frame);

    size_t fs = input_frame->header.data_size + sizeof(BleIntercomFrameHeader);

    furi_assert(fs <= instance->frame_size);

    if(ble_service_lock_frame(instance)) {
        memcpy(instance->frame_buf, input_frame, fs);
        ble_service_enqueue_message(instance, BleCommandServiceProcessFrame, NULL, 0);
    }
}

void ble_service_enqueue_message(
    BleServiceObject* instance,
    BleCommand command,
    void* data,
    uint8_t data_size) {
    furi_assert(instance);

    BleMessage msg = {.type = command, .service_index = instance->desc->index};
    furi_assert(data_size <= sizeof(msg.data));
    memcpy(msg.data, data, data_size);

    if(furi_message_queue_put(instance->message_queue, &msg, 100) != FuriStatusOk) {
        FURI_LOG_W(instance->desc->name, "Unable to enqueue for processing");
    }
}

void ble_service_set_state(BleServiceObject* instance, BleServiceState new_state) {
    furi_assert(instance);
    BLE_LOG_D("Require new_state: %d", new_state);
    if(ble_service_lock(instance)) {
        ble_service_enqueue_message(
            instance, BleCommandServiceSetState, &new_state, sizeof(BleServiceState));
        ble_service_unlock(instance);
    }
}
