#include "ble_service_i.h"
#include "target/ble_service_target.h"

#define TAG "BleServiceBase"

/* inline  */ bool ble_service_lock(BleServiceObject* instance) {
    if(furi_mutex_acquire(instance->service_lock, 100) != FuriStatusOk) {
        FURI_LOG_W(instance->desc->name, "Service lock failed");
        return false;
    }
    return true;
}

/* inline  */ void ble_service_unlock(BleServiceObject* instance) {
    if(furi_mutex_release(instance->service_lock) != FuriStatusOk) {
        FURI_LOG_W(instance->desc->name, "Service unlock failed");
    }
}

static bool ble_service_lock_input_frame(BleServiceObject* instance) {
    if(furi_semaphore_acquire(instance->frame_lock, 100) != FuriStatusOk) {
        FURI_LOG_W(instance->desc->name, "Frame lock failed");
        return false;
    }
    return true;
}

static void ble_service_unlock_input_frame(BleServiceObject* instance) {
    if(furi_semaphore_release(instance->frame_lock) != FuriStatusOk) {
        FURI_LOG_W(instance->desc->name, "Frame unlock failed");
    }
}

void ble_service_prepare_frame(
    BleServiceObject* instance,
    BleIntercomFrameType frame_type,
    uint8_t command_event,
    size_t data_size,
    void* data) {
    BleIntercomFrameGeneric* frame = (BleIntercomFrameGeneric*)instance->output;
    frame->header.frame_type = frame_type;
    frame->header.command = command_event;
    frame->header.service_index = instance->desc->index;
    frame->header.data_size = data_size;
    ///TODO: need more checks if there_is_enough memory in buffer
    if(data_size && data) memcpy(frame->data, data, data_size);
}

void ble_service_send_intercom_frame(BleServiceObject* instance) {
    const BleIntercomFrameGeneric* frame = (BleIntercomFrameGeneric*)instance->output;
    const BleIntercomFrameHeader* header = &frame->header;

    size_t frame_size = header->data_size + sizeof(BleIntercomFrameHeader);

    FURI_LOG_D(
        instance->desc->name,
        "Tx Frame t: %d c: %d ds: %d fs: %d",
        header->frame_type,
        header->command,
        header->data_size,
        frame_size);

    size_t tx =
        intercom_tx(instance->intercom, IntercomChannelBle, instance->output, frame_size, 100);
    furi_assert(tx == frame_size);
}

void ble_service_switch_state(BleServiceObject* instance, BleServiceState new_state) {
    // if(ble_service_switch_state_allowed(instance->state, new_state)) {

    FURI_LOG_D(instance->desc->name, "Set new_state: %d", new_state);
    instance->state = new_state;
    ///TODO:Move code below to some other place, because state switching may happen by remote request
    ///or by internal. In first case we need to send response and in second one - notification
    // if(notify_remote) {
    //     BleCommandEvent c = {.event = BleEventStateChanged};
    //     ble_service_send_intercom_frame(
    //         instance, BleIntercomFrameTypeNotification, c, sizeof(BleServiceState), &new_state);
    // }
    // }
}

bool execute_handler(BleServiceObject* instance, BleCommand command) {
    bool result = false;
    switch(command) {
    case BleCommandServiceInit:
        result = ble_service_target_init(instance);
        break;
    case BleCommandServiceRead:
        break;
    case BleCommandServiceWrite:
        break;
    case BleCommandServiceNotify:
        break;
    default:
        break;
    }

    ble_service_send_intercom_frame(instance);
    return result;
}

static bool ble_service_process_request(BleServiceObject* instance) {
    FURI_LOG_D(instance->desc->name, "Process request");
    BleIntercomFrameGeneric* frame = (BleIntercomFrameGeneric*)instance->frame_buf;
    bool result = false;

    result = execute_handler(instance, frame->header.command);

    // if(result) {
    //     ble_service_prepare_frame(
    //         instance, BleIntercomFrameTypeResponse, frame->header.command, 0, NULL);
    // }

    return result;

    // ble_service_target_process_request(instance);
    // ble_service_send_intercom_frame(instance);
    // BleIntercomFrameGeneric* frame = (BleIntercomFrameGeneric*)instance->frame_buf;

    //extract command type from frame
    //check if command available in current state
    //execute target specific command handler -> execute service specific handler

    //if(init_result)
    //ble_service_send_intercom_frame(instance); // send response with result to remote
    //else error?
}

static bool ble_service_process_response(BleServiceObject* instance) {
    FURI_LOG_D(instance->desc->name, "Process response");
    return ble_service_target_process_response(instance);
    //extract command type from frame
    //if(command == pending_command)
    //perform target specific response handler -> execute service specific if present
    //else
    //Error wrong packet received;
}

static bool ble_service_process_notification(BleServiceObject* instance) {
    FURI_LOG_D(instance->desc->name, "Notification frame RX");

    BleIntercomFrameGeneric* frame = (BleIntercomFrameGeneric*)instance->frame_buf;

    if(frame->header.command == BleCommandServiceNotify) {
        BleCharacteristicData* ch_data = (BleCharacteristicData*)frame->data;
        ble_service_target_notify(
            instance, ch_data->header.index, ch_data->data, ch_data->header.data_size);
    }

    //extract command type from frame
    //check if command available in current state
    //execute target specific command handler -> execute service specific handler
    return true;
}

static bool ble_service_process_input_frame(BleServiceObject* instance) {
    FURI_LOG_D(instance->desc->name, "process_input_frame");
    BleIntercomFrameGeneric* frame = (BleIntercomFrameGeneric*)instance->frame_buf;
    if(frame->header.frame_type == BleIntercomFrameTypeRequest) {
        ble_service_process_request(instance);
        // if(frame->header.command == BleCommandServiceInit) {
        //     ble_service_target_init(instance);
        //     ble_service_send_intercom_frame(instance);
        // }
    } else if(frame->header.frame_type == BleIntercomFrameTypeResponse) {
        FURI_LOG_W(instance->desc->name, "State: %d", instance->state);
        ble_service_process_response(instance);
    } else if(frame->header.frame_type == BleIntercomFrameTypeNotification) {
        ble_service_process_notification(instance);
    }

    ble_service_unlock_input_frame(instance);
    return true;
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

bool ble_service_process(BleServiceObject* instance, const BleMessage* msg) {
    furi_assert(instance);
    furi_assert(msg);

    FURI_LOG_D(instance->desc->name, "ble_service_process");
    bool result = false;
    if(ble_service_lock(instance)) {
        if(msg->type == BleCommandServiceProcessFrame) {
            result = ble_service_process_input_frame(instance);
        } else
            result = execute_handler(instance, msg->type);

        ble_service_unlock(instance);
    }
    return result;
}

void ble_service_process_mailbox(BleServiceObject* instance, BleIntercomFrameGeneric* input_frame) {
    furi_assert(instance);
    furi_assert(input_frame);

    size_t fs = input_frame->header.data_size + sizeof(BleIntercomFrameHeader);

    furi_assert(fs <= instance->frame_size);

    if(ble_service_lock_input_frame(instance)) {
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

void ble_service_eqnueue_init(BleServiceObject* instance) {
    furi_assert(instance);
    FURI_LOG_D(instance->desc->name, "Enqueue init");
    if(ble_service_lock(instance)) {
        ble_service_enqueue_message(instance, BleCommandServiceInit, NULL, 0);
        ble_service_unlock(instance);
    }
}
