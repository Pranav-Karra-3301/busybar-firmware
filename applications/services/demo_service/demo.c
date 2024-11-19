#include <furi.h>

#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>

#define TAG "DemoSrv"

#ifndef ENABLE_USART0
#define ENABLE_USART0 (1)
#endif

#ifndef ENABLE_DMA
#define ENABLE_DMA (1)
#endif

#define BUFFER_SIZE    (1024UL)
#define UART_BAUD_RATE (5625000UL)

typedef struct {
    FuriHalSerialHandle* handle;
    FuriSemaphore* semaphore;
    uint8_t buffer[BUFFER_SIZE];
    size_t data_len;
} DemoServiceSerialContext;

typedef struct {
    FuriEventLoop* event_loop;
    DemoServiceSerialContext serial_context[2];
} DemoService;

static void demo_service_semaphore_callback(FuriEventLoopObject* object, void* ctx) {
    DemoServiceSerialContext* context = ctx;
    furi_check(object == context->semaphore);

    furi_check(furi_semaphore_acquire(context->semaphore, 0) == FuriStatusOk);

#if ENABLE_DMA
    furi_hal_serial_dma_tx(context->handle, context->buffer, BUFFER_SIZE);
#else
    if(context->data_len) {
        furi_hal_serial_tx(context->handle, context->buffer, context->data_len);
        furi_hal_serial_tx_wait_complete(context->handle);
    }
#endif
}

#if ENABLE_DMA
static void demo_service_serial_rx_callback(
    FuriHalSerialHandle* handle,
    FuriHalSerialRxEvent event,
    void* ctx) {
    DemoServiceSerialContext* context = ctx;

    if(event & FuriHalSerialRxEventData) {
        furi_hal_serial_dma_rx_start(handle, context->buffer, BUFFER_SIZE);
        furi_semaphore_release(context->semaphore);

    } else {
        furi_crash();
    }
}
#else
static void demo_service_serial_rx_callback(
    FuriHalSerialHandle* handle,
    FuriHalSerialRxEvent event,
    void* ctx) {
    DemoServiceSerialContext* context = ctx;

    if(event & FuriHalSerialRxEventData || event & FuriHalSerialRxEventIdle) {
        context->data_len = furi_hal_serial_async_rx(handle, context->buffer, BUFFER_SIZE);
    }
    if(event & FuriHalSerialRxEventBreak) {
        furi_crash("Break Condition");
    }
    if(event & FuriHalSerialRxEventOverrunError) {
        furi_crash("Overrun Error");
    }
    if(event & FuriHalSerialRxEventFrameError) {
        furi_crash("Framing Error");
    }

    furi_semaphore_release(context->semaphore);
}
#endif

static void demo_service_init_serial_context(DemoService* instance, FuriHalSerialId id) {
    DemoServiceSerialContext* context = &instance->serial_context[id];
    context->semaphore = furi_semaphore_alloc(1, 0);

    furi_event_loop_subscribe_semaphore(
        instance->event_loop,
        context->semaphore,
        FuriEventLoopEventIn,
        demo_service_semaphore_callback,
        context);

    context->handle = furi_hal_serial_control_acquire(id);
    furi_check(context->handle);

    furi_hal_serial_init(context->handle, UART_BAUD_RATE);
    furi_hal_serial_set_callback(context->handle, NULL, demo_service_serial_rx_callback, context);

#if ENABLE_DMA
    furi_hal_serial_dma_rx_start(context->handle, context->buffer, BUFFER_SIZE);
#else
    furi_hal_serial_async_rx_start(context->handle, true);
#endif
}

static DemoService* demo_service_alloc(void) {
    DemoService* instance = malloc(sizeof(DemoService));

    instance->event_loop = furi_event_loop_alloc();

#if ENABLE_USART0
    demo_service_init_serial_context(instance, FuriHalSerialIdUsart0);
#endif
#if ENABLE_UART1
    demo_service_init_serial_context(instance, FuriHalSerialIdUart1);
#endif

    return instance;
}

int32_t demo_srv(void* arg) {
    UNUSED(arg);

    DemoService* instance = demo_service_alloc();
    furi_event_loop_run(instance->event_loop);

    furi_crash();
}
