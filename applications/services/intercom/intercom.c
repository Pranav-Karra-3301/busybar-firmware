#include "intercom.h"

#include <furi.h>

#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>

#include "intercom_decoder.h"
#include "intercom_encoder.h"

#define TAG "IntercomSrv"

#ifndef INTERCOM_BAUD_RATE
#define INTERCOM_BAUD_RATE (921600UL)
#endif

#ifndef INTERCOM_BUFFER_SIZE
#define INTERCOM_BUFFER_SIZE (INTERCOM_FRAME_MAX_SIZE)
#endif

// TODO: Reduce timeout to absolute minimum
#define INTERCOM_CONFIRM_TIMEOUT_MS (1000U)

struct Intercom {
    FuriEventLoop* event_loop;
    FuriEventLoopTimer* rx_timer;
    FuriEventLoopTimer* tx_timer;
    FuriStreamBuffer* rx_buffer;
    FuriStreamBuffer* tx_buffer;
    FuriHalSerialHandle* serial;
    IntercomDecoder* decoder;
    IntercomEncoder* encoder;
    IntercomRxCallback rx_callback;
    void* rx_callback_context;
};

static void intercom_serial_rx_callback(
    FuriHalSerialHandle* handle,
    FuriHalSerialRxEvent event,
    void* context) {
    Intercom* instance = context;

    if(event & FuriHalSerialRxEventData || event & FuriHalSerialRxEventIdle) {
        while(furi_hal_serial_async_rx_available(handle)) {
            const uint8_t byte = furi_hal_serial_async_rx(handle);
            furi_check(furi_stream_buffer_send(instance->rx_buffer, &byte, 1, 0) == 1);
        }

    // TODO: Handle Serial errors
    } else if(event & FuriHalSerialRxEventFrameError) {
        FURI_LOG_E(TAG, "Frame Error");
    } else if(event & FuriHalSerialRxEventNoiseError) {
        FURI_LOG_E(TAG, "Noise Error");
    } else if(event & FuriHalSerialRxEventOverrunError) {
        FURI_LOG_E(TAG, "Overrun Error");
    }
}

static void intercom_rx_buffer_callback(FuriEventLoopObject* object, void* context) {
    Intercom* instance = context;
    furi_assert(object == instance->rx_buffer);

    while(furi_stream_buffer_bytes_available(instance->rx_buffer)) {
        uint8_t byte;
        furi_check(furi_stream_buffer_receive(instance->rx_buffer, &byte, 1, 0) == 1);

        const IntercomDecoderResult status = intercom_decoder_feed(instance->decoder, byte);

        if(status == IntercomDecoderResultDone) {
            const IntercomFrameHeader* header = intercom_decoder_get_header(instance->decoder);

            if(header->flags & IntercomFrameFlagData) {
                // Schedule confirmation of receipt - either with an outgoing frame or on its own
                furi_event_loop_timer_start(instance->rx_timer, INTERCOM_CONFIRM_TIMEOUT_MS / 2);

                const IntercomFramePayload* payload =
                    intercom_decoder_get_payload(instance->decoder);
                instance->rx_callback(payload->data, payload->size, instance->rx_callback_context);
            }
            if(header->flags & IntercomFrameFlagConfirm) {
                furi_event_loop_timer_stop(instance->rx_timer);
            }
            if(header->flags & IntercomFrameFlagError) {
                FURI_LOG_E(TAG, "Receive Error");
                // TODO: Resend previous frame
                furi_event_loop_timer_stop(instance->rx_timer);
            }

            intercom_decoder_reset(instance->decoder);

        } else if(status == IntercomDecoderResultError) {
            FURI_LOG_E(TAG, "Decode Error");
            // TODO: Send error frame right away
            intercom_decoder_reset(instance->decoder);
        }
    }
}

void intercom_set_rx_callback(Intercom* instance, IntercomRxCallback callback, void* context) {
    furi_check(instance);
    furi_check(callback);
    furi_check(instance->rx_callback == NULL);

    instance->rx_callback_context = context;
    instance->rx_callback = callback;
}

// One outgoing frame per callback invocation
static void intercom_tx_buffer_callback(FuriEventLoopObject* object, void* context) {
    Intercom* instance = context;
    furi_assert(object == instance->tx_buffer);

    uint16_t frame_flags = IntercomFrameFlagData;

    // Integrate the confirmation of receipt to this frame
    if(furi_event_loop_timer_is_running(instance->rx_timer)) {
        furi_event_loop_timer_stop(instance->rx_timer);
        frame_flags |= IntercomFrameFlagConfirm;
    }

    intercom_encoder_begin_frame(instance->encoder, 0, frame_flags);

    const size_t bytes_available =
        MIN(furi_stream_buffer_bytes_available(instance->tx_buffer), INTERCOM_FRAME_MAX_SIZE);
    IntercomFramePayload* payload =
        intercom_encoder_allocate_payload(instance->encoder, bytes_available);

    furi_check(
        furi_stream_buffer_receive(instance->tx_buffer, payload->data, payload->size, 0) ==
        bytes_available);
    intercom_encoder_finalize_frame(instance->encoder);

    size_t frame_size;
    const uint8_t* frame_data = intercom_encoder_get_frame_data(instance->encoder, &frame_size);

    furi_hal_serial_tx(instance->serial, frame_data, frame_size);
    furi_hal_serial_tx_wait_complete(instance->serial);

    // Start waiting for confirmation from the receiving side
    furi_event_loop_timer_start(instance->tx_timer, INTERCOM_CONFIRM_TIMEOUT_MS);
}

// Called if it was not possible to integrate the confirmation into an outgoing frame
static void intercom_rx_timer_callback(void* context) {
    Intercom* instance = context;

    // Send confirmation frame with no other data
    intercom_encoder_begin_frame(instance->encoder, 0, IntercomFrameFlagConfirm);
    intercom_encoder_finalize_frame(instance->encoder);

    size_t frame_size;
    const uint8_t* frame_data = intercom_encoder_get_frame_data(instance->encoder, &frame_size);

    furi_hal_serial_tx(instance->serial, frame_data, frame_size);
    furi_hal_serial_tx_wait_complete(instance->serial);
}

// Called upon outgoing frame confirmation timeout
static void intercom_tx_timer_callback(void* context) {
    Intercom* instance = context;

    // Send error frame with no other data
    intercom_encoder_begin_frame(instance->encoder, 0, IntercomFrameFlagConfirm);
    intercom_encoder_finalize_frame(instance->encoder);

    size_t frame_size;
    const uint8_t* frame_data = intercom_encoder_get_frame_data(instance->encoder, &frame_size);

    furi_hal_serial_tx(instance->serial, frame_data, frame_size);
    furi_hal_serial_tx_wait_complete(instance->serial);
}

static Intercom* intercom_alloc(void) {
    Intercom* instance = malloc(sizeof(Intercom));

    instance->event_loop = furi_event_loop_alloc();
    instance->rx_timer = furi_event_loop_timer_alloc(
        instance->event_loop, intercom_rx_timer_callback, FuriEventLoopTimerTypeOnce, instance);
    instance->tx_timer = furi_event_loop_timer_alloc(
        instance->event_loop, intercom_tx_timer_callback, FuriEventLoopTimerTypeOnce, instance);
    instance->rx_buffer = furi_stream_buffer_alloc(INTERCOM_BUFFER_SIZE, 1);
    instance->tx_buffer = furi_stream_buffer_alloc(INTERCOM_BUFFER_SIZE, 1);

    furi_event_loop_subscribe_stream_buffer(
        instance->event_loop,
        instance->rx_buffer,
        FuriEventLoopEventIn,
        intercom_rx_buffer_callback,
        instance);

    furi_event_loop_subscribe_stream_buffer(
        instance->event_loop,
        instance->rx_buffer,
        FuriEventLoopEventIn,
        intercom_tx_buffer_callback,
        instance);

    instance->decoder = intercom_decoder_alloc();
    instance->encoder = intercom_encoder_alloc();

    instance->serial = furi_hal_serial_control_acquire(FuriHalSerialIdUsart0);
    furi_hal_serial_set_br(instance->serial, INTERCOM_BAUD_RATE);
    furi_hal_serial_async_rx_start(instance->serial, intercom_serial_rx_callback, instance, true);

    furi_record_create(RECORD_INTERCOM, instance);

    return instance;
}

size_t intercom_tx(Intercom* instance, const void* data, size_t data_size, uint32_t timeout) {
    furi_check(instance);
    furi_check(data);
    furi_check(data_size > 0);

    size_t sent_data_size = 0;

    const uint32_t start_time = furi_get_tick();
    uint32_t remaining_time = timeout;

    do {
        const size_t chunk_size = MIN(data_size - sent_data_size, INTERCOM_BUFFER_SIZE);
        const size_t sent_chunk_size = furi_stream_buffer_send(
            instance->tx_buffer, data + sent_data_size, chunk_size, remaining_time);

        sent_data_size += sent_chunk_size;

        const uint32_t elapsed_time = furi_get_tick() - start_time;

        if(elapsed_time > remaining_time) {
            remaining_time = 0;
        } else {
            remaining_time -= elapsed_time;
        }

    } while(remaining_time > 0 && sent_data_size < data_size);

    return sent_data_size;
}

int32_t intercom_srv(void* arg) {
    UNUSED(arg);

    Intercom* instance = intercom_alloc();
    furi_event_loop_run(instance->event_loop);

    furi_crash();
}
