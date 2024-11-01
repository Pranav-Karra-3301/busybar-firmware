#include <furi.h>

#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>

#define TAG "DemoSrv"

#define UART_BAUD_RATE (11250000UL)

#define STREAM_BUFFER_SIZE (16UL)

typedef struct {
    FuriHalSerialHandle* handle;
    FuriStreamBuffer* stream_buffer;
} DemoServiceSerialContext;

typedef struct {
    FuriEventLoop* event_loop;
    DemoServiceSerialContext serial_context[2];
} DemoService;

static void demo_service_stream_buffer_callback(FuriEventLoopObject* object, void* ctx) {
    DemoServiceSerialContext* context = ctx;
    furi_check(object == context->stream_buffer);

    char data[STREAM_BUFFER_SIZE + 1] = {};

    const uint32_t bytes_available = furi_stream_buffer_bytes_available(context->stream_buffer);
    furi_check(
        furi_stream_buffer_receive(context->stream_buffer, data, bytes_available, 0) ==
        bytes_available);

    furi_hal_serial_tx(context->handle, (const uint8_t*)data, bytes_available);
    furi_hal_serial_tx_wait_complete(context->handle);
}

static void demo_service_serial_rx_callback(
    FuriHalSerialHandle* handle,
    FuriHalSerialRxEvent event,
    void* ctx) {
    DemoServiceSerialContext* context = ctx;

    if(event & (FuriHalSerialRxEventData | FuriHalSerialRxEventIdle)) {
        while(furi_hal_serial_async_rx_available(handle)) {
            const uint8_t c = furi_hal_serial_async_rx(handle);
            furi_check(
                furi_stream_buffer_send(context->stream_buffer, &c, sizeof(c), 0) == sizeof(c));
        }
    } else {
        furi_crash();
    }
}

static void demo_service_init_serial_context(DemoService* instance, FuriHalSerialId id) {
    DemoServiceSerialContext* context = &instance->serial_context[id];

    context->stream_buffer = furi_stream_buffer_alloc(STREAM_BUFFER_SIZE, 1);
    furi_event_loop_subscribe_stream_buffer(
        instance->event_loop,
        context->stream_buffer,
        FuriEventLoopEventIn,
        demo_service_stream_buffer_callback,
        context);

    context->handle = furi_hal_serial_control_acquire(id);
    furi_check(context->handle);

    furi_hal_serial_init(context->handle, UART_BAUD_RATE);
    furi_hal_serial_async_rx_start(
        context->handle, demo_service_serial_rx_callback, context, false);
}

static DemoService* demo_service_alloc(void) {
    DemoService* instance = malloc(sizeof(DemoService));

    instance->event_loop = furi_event_loop_alloc();

    demo_service_init_serial_context(instance, FuriHalSerialIdUsart0);
    // Uncomment for echo on UART1, beware of parasitic signals
    // demo_service_init_serial_context(instance, FuriHalSerialIdUart1);

    return instance;
}

int32_t demo_srv(void* arg) {
    UNUSED(arg);

    DemoService* instance = demo_service_alloc();
    furi_event_loop_run(instance->event_loop);

    furi_crash();
}
