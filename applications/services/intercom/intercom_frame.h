/**
 * @file intercom_protocol.h
 * @brief Frame definitions and parsing for the Intercom service
 */
#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define INTERCOM_FRAME_MAX_SIZE           (256U)
#define INTERCOM_FRAME_MIN_SIZE           (sizeof(IntercomFrameHeader) + sizeof(IntercomFrameTrailer))
#define INTERCOM_FRAME_MIN_SIZE_W_PAYLOAD (INTERCOM_FRAME_MIN_SIZE + sizeof(IntercomFramePayload))
#define INTERCOM_FRAME_MAX_PAYLOAD_SIZE \
    (INTERCOM_FRAME_MAX_SIZE - INTERCOM_FRAME_MIN_SIZE_W_PAYLOAD)

typedef enum {
    IntercomFrameFlagData = 1U << 0,
    IntercomFrameFlagConfirm = 1U << 1,
    IntercomFrameFlagError = 1U << 2,
} IntercomFrameFlag;

#pragma pack(push, 1)

typedef struct {
    uint16_t id;
    uint16_t flags;
} IntercomFrameHeader;

typedef struct {
    uint16_t size;
    uint8_t data[];
} IntercomFramePayload;

typedef struct {
    IntercomFrameHeader header;
    IntercomFramePayload payload;
} IntercomFrame;

typedef struct {
    uint16_t check;
} IntercomFrameTrailer;

#pragma pack(pop)

#ifdef __cplusplus
}
#endif
