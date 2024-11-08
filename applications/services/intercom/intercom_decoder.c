#include "intercom_decoder.h"

#include <furi.h>

#include "intercom_cksum.h"

#define TAG "IntercomDecoder"

struct IntercomDecoder {
    uint8_t data[INTERCOM_FRAME_MAX_SIZE];
    size_t size;
};

IntercomDecoder* intercom_decoder_alloc(void) {
    IntercomDecoder* instance = malloc(sizeof(IntercomDecoder));
    return instance;
}

void intercom_decoder_reset(IntercomDecoder* instance) {
    furi_check(instance);
    memset(instance->data, 0, INTERCOM_FRAME_MAX_SIZE);
    instance->size = 0;
}

static FURI_ALWAYS_INLINE IntercomDecoderResult
    intercom_decoder_contains_valid_frame(IntercomDecoder* instance) {
    IntercomDecoderResult ret;

    do {
        if(instance->size < INTERCOM_FRAME_MIN_SIZE) {
            ret = IntercomDecoderResultIncomplete;
            break;
        }

        const IntercomFrameHeader* header = (const IntercomFrameHeader*)instance->data;

        size_t payload_size = 0;

        if(header->flags & IntercomFrameFlagData) {
            if(instance->size < INTERCOM_FRAME_MIN_SIZE_W_PAYLOAD) {
                ret = IntercomDecoderResultIncomplete;
                break;
            }

            const IntercomFrame* frame = (const IntercomFrame*)instance->data;

            if(instance->size < INTERCOM_FRAME_MIN_SIZE_W_PAYLOAD + frame->payload.size) {
                ret = IntercomDecoderResultIncomplete;
                break;

            } else if(frame->payload.size > INTERCOM_FRAME_MAX_PAYLOAD_SIZE) {
                ret = IntercomDecoderResultError;
                break;
            }

            payload_size = sizeof(IntercomFramePayload) + frame->payload.size;
        }

        const size_t trailer_offset = sizeof(IntercomFrameHeader) + payload_size;
        const IntercomFrameTrailer* trailer =
            (const IntercomFrameTrailer*)(instance->data + trailer_offset);
        const uint16_t cksum = intercom_calculate_cksum(
            instance->data, instance->size - sizeof(IntercomFrameTrailer));

        if(trailer->check != cksum) {
            ret = IntercomDecoderResultError;
            break;
        }

        ret = IntercomDecoderResultDone;
    } while(false);

    return ret;
}

IntercomDecoderResult intercom_decoder_feed(IntercomDecoder* instance, uint8_t byte) {
    furi_check(instance);
    furi_check(instance->size < INTERCOM_FRAME_MAX_SIZE);

    instance->data[instance->size++] = byte;

    return intercom_decoder_contains_valid_frame(instance);
}

const IntercomFrameHeader* intercom_decoder_get_header(IntercomDecoder* instance) {
    furi_check(instance);
    furi_check(instance->size >= INTERCOM_FRAME_MIN_SIZE);

    const IntercomFrame* frame = (const IntercomFrame*)instance->data;
    return &frame->header;
}

const IntercomFramePayload* intercom_decoder_get_payload(IntercomDecoder* instance) {
    furi_check(instance);
    furi_check(instance->size >= INTERCOM_FRAME_MIN_SIZE_W_PAYLOAD);

    const IntercomFrame* frame = (const IntercomFrame*)instance->data;
    return &frame->payload;
}
