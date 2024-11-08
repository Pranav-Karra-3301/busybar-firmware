#pragma once

#include "intercom_frame.h"

typedef struct IntercomEncoder IntercomEncoder;

IntercomEncoder* intercom_encoder_alloc(void);

void intercom_encoder_begin_frame(IntercomEncoder* instance, uint16_t id, uint16_t flags);

IntercomFramePayload*
    intercom_encoder_allocate_payload(IntercomEncoder* instance, size_t payload_size);

void intercom_encoder_finalize_frame(IntercomEncoder* instance);

const uint8_t* intercom_encoder_get_frame_data(const IntercomEncoder* instance, size_t* data_size);
