#include "intercom_encoder.h"

#include <furi.h>

#include "intercom_cksum.h"

struct IntercomEncoder {
    uint8_t data[INTERCOM_FRAME_MAX_SIZE];
    size_t size;
};

IntercomEncoder* intercom_encoder_alloc(void) {
    IntercomEncoder* instance = malloc(sizeof(IntercomEncoder));
    return instance;
}

void intercom_encoder_begin_frame(IntercomEncoder* instance, uint16_t id, uint16_t flags) {
    furi_check(instance);

    IntercomFrameHeader* header = (IntercomFrameHeader*)instance->data;
    header->id = id;
    header->flags = flags;

    instance->size = sizeof(IntercomFrameHeader);
}

IntercomFramePayload*
    intercom_encoder_allocate_payload(IntercomEncoder* instance, size_t payload_size) {
    furi_check(instance);

    IntercomFrame* frame = (IntercomFrame*)instance->data;
    IntercomFramePayload* payload = &frame->payload;
    payload->size = payload_size;

    instance->size = sizeof(IntercomFrame) + payload_size;
    return payload;
}

void intercom_encoder_finalize_frame(IntercomEncoder* instance) {
    furi_check(instance);

    IntercomFrameTrailer* trailer = (IntercomFrameTrailer*)(instance->data + instance->size);
    trailer->check = intercom_calculate_cksum(instance->data, instance->size);

    instance->size += sizeof(IntercomFrameTrailer);
}

const uint8_t*
    intercom_encoder_get_frame_data(const IntercomEncoder* instance, size_t* data_size) {
    furi_check(instance);
    furi_check(data_size);

    *data_size = instance->size;
    return instance->data;
}
