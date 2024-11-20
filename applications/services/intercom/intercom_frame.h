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

#define INTERCOM_MIN_FRAME_SIZE (sizeof(IntercomFrameHeader) + sizeof(IntercomFrameTrailer))

#define INTERCOM_S_FRAME_SIZE         (INTERCOM_MIN_FRAME_SIZE)
#define INTERCOM_D_FRAME_SIZE         (1024U)
#define INTERCOM_D_FRAME_PAYLOAD_SIZE (INTERCOM_D_FRAME_SIZE - INTERCOM_MIN_FRAME_SIZE)
#define INTERCOM_D_FRAME_DATA_SIZE    (INTERCOM_D_FRAME_PAYLOAD_SIZE - sizeof(uint16_t))

typedef enum {
    IntercomFrameFlagData = 1U << 0,
    IntercomFrameFlagService = 1U << 1,
} IntercomFrameFlag;

#pragma pack(push, 1)

typedef struct {
    uint8_t id;
    uint8_t flags;
} IntercomFrameHeader;

typedef struct {
    uint16_t check;
} IntercomFrameTrailer;

typedef struct {
    uint16_t size;
    uint8_t data[INTERCOM_D_FRAME_DATA_SIZE];
} IntercomFramePayload;

typedef struct {
    IntercomFrameHeader header;
    union {
        struct {
            IntercomFramePayload payload;
            IntercomFrameTrailer trailer;
        } d;
        struct {
            IntercomFrameTrailer trailer;
        } s;
    };
} IntercomFrame;

#pragma pack(pop)

static_assert(sizeof(IntercomFrame) == INTERCOM_D_FRAME_SIZE);

static inline uint16_t intercom_frame_calculate_checksum(const IntercomFrame* frame) {
    (void)frame;
    // TODO: Decide on the algorithm
    return 0xa1a1;
}

#ifdef __cplusplus
}
#endif
