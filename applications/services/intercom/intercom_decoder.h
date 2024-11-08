#pragma once

#include "intercom_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    IntercomDecoderResultDone,
    IntercomDecoderResultIncomplete,
    IntercomDecoderResultError,
} IntercomDecoderResult;

typedef struct IntercomDecoder IntercomDecoder;

IntercomDecoder* intercom_decoder_alloc(void);

void intercom_decoder_reset(IntercomDecoder* instance);

IntercomDecoderResult intercom_decoder_feed(IntercomDecoder* instance, uint8_t byte);

const IntercomFrameHeader* intercom_decoder_get_header(IntercomDecoder* instance);

const IntercomFramePayload* intercom_decoder_get_payload(IntercomDecoder* instance);

#ifdef __cplusplus
}
#endif
