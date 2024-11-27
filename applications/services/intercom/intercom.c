#include "intercom.h"

#include <furi.h>

#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>

#include "intercom_frame.h"

#define TAG "IntercomSrv"

#ifndef INTERCOM_BAUD_RATE
#define INTERCOM_BAUD_RATE (115200UL)
#endif

// TODO: Reduce timeout to absolute minimum
#define INTERCOM_RESPONSE_TIMEOUT_MS (250U)

#ifdef STM32U595xx
#define INTERCOM_SERIAL FuriHalSerialIdUsart1
#else
#define INTERCOM_SERIAL FuriHalSerialIdUsart0
#endif

typedef enum {
    IntercomStateIdle,
    IntercomStateWaitingForResponse,
} IntercomState;

typedef enum {
    IntercomEventData = 1UL << 0,
    IntercomEventFrameSent = 1UL << 1,
    IntercomEventFrameReceived = 1UL << 2,
} IntercomEvent;

struct Intercom {
    FuriEventLoop* event_loop;
    FuriSemaphore* tx_semaphore;
    FuriEventLoopTimer* response_timer;
    FuriHalSerialHandle* serial;
    IntercomRxCallback rx_callback;
    void* callback_context;
    IntercomFrame tx_frame;
    IntercomFrame rx_frame;
    IntercomState state;
};

// Called in ISR context
static void intercom_serial_tx_callback(
    FuriHalSerialHandle* handle,
    FuriHalSerialTxEvent event,
    void* context) {
    UNUSED(handle);

    Intercom* instance = context;

    if(event & FuriHalSerialTxEventComplete) {
        furi_event_loop_set_custom_event(instance->event_loop, IntercomEventFrameSent);
    }
}

// Called in interrupt context
static void intercom_serial_rx_callback(
    FuriHalSerialHandle* handle,
    FuriHalSerialRxEvent event,
    void* context) {
    UNUSED(handle);

    Intercom* instance = context;

    if(event & FuriHalSerialRxEventData) {
        furi_event_loop_set_custom_event(instance->event_loop, IntercomEventFrameReceived);
    }
}

void intercom_set_rx_callback(Intercom* instance, IntercomRxCallback callback, void* context) {
    furi_check(instance);
    furi_check(callback);
    furi_check(instance->rx_callback == NULL);

    instance->callback_context = context;
    instance->rx_callback = callback;
}

static inline void intercom_send_data_frame(Intercom* instance) {
    IntercomFrame* tx_frame = &instance->tx_frame;

    tx_frame->header.id = 0;
    tx_frame->header.flags = IntercomFrameFlagData;
    tx_frame->d.trailer.check = intercom_frame_calculate_checksum(tx_frame);

    // Send request
    furi_hal_serial_dma_tx(instance->serial, (uint8_t*)tx_frame, INTERCOM_D_FRAME_SIZE);
}

static inline void intercom_send_response_frame(Intercom* instance) {
    IntercomFrame* tx_frame = &instance->tx_frame;

    tx_frame->header.id = 0;
    tx_frame->header.flags = IntercomFrameFlagService;
    tx_frame->s.trailer.check = intercom_frame_calculate_checksum(tx_frame);

    // Send confirmation
    furi_hal_serial_dma_tx(instance->serial, (uint8_t*)tx_frame, INTERCOM_S_FRAME_SIZE);
}

static inline void intercom_process_rx_frame(Intercom* instance) {
    if(instance->state == IntercomStateIdle) {
        if(instance->rx_frame.header.flags & IntercomFrameFlagData) {
            if(instance->rx_callback) {
                instance->rx_callback(
                    instance->rx_frame.d.payload.data,
                    instance->rx_frame.d.payload.size,
                    instance->callback_context);
            }

            intercom_send_response_frame(instance);

        } else {
            // Unexpected frame
            furi_crash();
        }

    } else if(instance->state == IntercomStateWaitingForResponse) {
        furi_event_loop_timer_stop(instance->response_timer);

        if(instance->rx_frame.header.flags & IntercomFrameFlagService) {
            instance->state = IntercomStateIdle;
            furi_hal_serial_dma_rx_start(
                instance->serial, (uint8_t*)&instance->rx_frame, INTERCOM_D_FRAME_SIZE);
            furi_semaphore_release(instance->tx_semaphore);

        } else {
            // Unexpected frame
            furi_crash();
        }
    }
}

static inline void intercom_process_tx_frame(Intercom* instance) {
    // Start waiting for the response
    furi_hal_serial_dma_rx_start(
        instance->serial, (uint8_t*)&instance->rx_frame, INTERCOM_S_FRAME_SIZE);
    furi_event_loop_timer_start(instance->response_timer, INTERCOM_RESPONSE_TIMEOUT_MS);
}

static void intercom_custom_event_callback(uint32_t events, void* context) {
    Intercom* instance = context;
    if(events & IntercomEventData) {
        intercom_send_data_frame(instance);
    }
    if(events & IntercomEventFrameReceived) {
        intercom_process_rx_frame(instance);
    }
    if(events & IntercomEventFrameSent) {
        intercom_process_tx_frame(instance);
    }
}

static void intercom_response_timer_callback(void* context) {
    Intercom* instance = context;

    FURI_LOG_E(TAG, "Response timeout");
    // Resend the frame
    intercom_send_data_frame(instance);
}

static Intercom* intercom_alloc(void) {
    Intercom* instance = malloc(sizeof(Intercom));

    instance->event_loop = furi_event_loop_alloc();

    instance->tx_semaphore = furi_semaphore_alloc(1, 1);
    instance->response_timer = furi_event_loop_timer_alloc(
        instance->event_loop,
        intercom_response_timer_callback,
        FuriEventLoopTimerTypeOnce,
        instance);

    instance->serial = furi_hal_serial_control_acquire(INTERCOM_SERIAL);
    furi_hal_serial_init(instance->serial, INTERCOM_BAUD_RATE);
    furi_hal_serial_set_callback(
        instance->serial, intercom_serial_tx_callback, intercom_serial_rx_callback, instance);
    // Start listening for D-frames right away
    furi_hal_serial_dma_rx_start(
        instance->serial, (uint8_t*)&instance->rx_frame, INTERCOM_D_FRAME_SIZE);

    furi_event_loop_set_custom_event_callback(
        instance->event_loop, intercom_custom_event_callback, instance);

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

    while(furi_semaphore_acquire(instance->tx_semaphore, remaining_time) == FuriStatusOk) {
        const size_t payload_size = MIN(data_size - sent_data_size, INTERCOM_D_FRAME_DATA_SIZE);

        memcpy(instance->tx_frame.d.payload.data, data + sent_data_size, payload_size);
        instance->tx_frame.d.payload.size = payload_size;

        furi_event_loop_set_custom_event(instance->event_loop, IntercomEventData);

        sent_data_size += payload_size;
        if(sent_data_size == data_size) break;

        const uint32_t elapsed_time = furi_get_tick() - start_time;
        if(elapsed_time >= remaining_time) break;

        remaining_time -= elapsed_time;
    }

    return sent_data_size;
}

int32_t intercom_srv(void* arg) {
    UNUSED(arg);

    Intercom* instance = intercom_alloc();
    furi_event_loop_run(instance->event_loop);

    furi_crash();
}
