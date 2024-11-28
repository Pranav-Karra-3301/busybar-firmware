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
    IntercomFrameTypeData,
    IntercomFrameTypeConfirm,
    IntercomFrameTypeError,
    IntercomFrameTypeMax,
} IntercomFrameType;

typedef enum {
    IntercomFrameErrorNone,
    IntercomFrameErrorFormat,
    IntercomFrameErrorWrongType,
    IntercomFrameErrorMax,
} IntercomFrameError;

#pragma pack(push, 1)

typedef struct {
    uint8_t id;
    uint8_t type  : 4;
    uint8_t error : 4;
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

static inline bool intercom_frame_is_valid(const IntercomFrame* frame) {
    bool is_valid = false;

    do {
        const IntercomFrameType type = frame->header.type;
        uint16_t checksum;

        if(type == IntercomFrameTypeData) {
            const uint16_t payload_size = frame->d.payload.size;
            if(payload_size > INTERCOM_D_FRAME_PAYLOAD_SIZE) {
                break;
            }

            checksum = frame->d.trailer.check;

        } else if(type == IntercomFrameTypeConfirm || type == IntercomFrameTypeError) {
            if(type == IntercomFrameTypeError) {
                if(frame->header.error >= IntercomFrameErrorMax) {
                    break;
                }
            }

            checksum = frame->s.trailer.check;

        } else {
            break;
        }

        if(intercom_frame_calculate_checksum(frame) != checksum) {
            break;
        }

        is_valid = true;
    } while(false);

    return is_valid;
}

#ifdef __cplusplus
}
#endif
