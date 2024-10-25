#include <furi.h>

#include <furi_hal_serial.h>
#include <furi_hal_resources.h>
#include <furi_hal_serial_control.h>

#define TAG "DemoSrv"

#define UART_BAUD_RATE (921600UL)
#define LOG_INTERVAL_MS (500UL)

typedef struct {
    FuriEventLoop* event_loop;
    FuriMessageQueue* message_queue;
    FuriHalSerialHandle* usart0;
    FuriHalSerialHandle* uart1;
    uint32_t counter;
} DemoService;

typedef enum {
    DemoButtonPomodoro,
    DemoButtonBusy,
    DemoButtonOff,
} DemoButton;

static void demo_service_pomodoro_int_callback(void* context) {
    DemoService* instance = context;
    const DemoButton button = DemoButtonPomodoro;
    // No checking, don't care if event is lost due queue being full
    furi_message_queue_put(instance->message_queue, &button, 0);
}

static void demo_service_busy_int_callback(void* context) {
    DemoService* instance = context;
    const DemoButton button = DemoButtonBusy;
    // No checking, don't care if event is lost due queue being full
    furi_message_queue_put(instance->message_queue, &button, 0);
}

static void demo_service_off_int_callback(void* context) {
    DemoService* instance = context;
    const DemoButton button = DemoButtonOff;
    // No checking, don't care if event is lost due queue being full
    furi_message_queue_put(instance->message_queue, &button, 0);
}

static void demo_service_tick_callback(void* context) {
    DemoService* instance = context;

    FURI_LOG_I(TAG, "Hello from ULPUART! %lu", instance->counter);

    FuriString* tmp = furi_string_alloc();

    furi_string_printf(tmp, "Hello from USART0! %lu\r\n", instance->counter);
    furi_hal_serial_tx(instance->usart0, (uint8_t*)furi_string_get_cstr(tmp), furi_string_size(tmp));
    furi_hal_serial_tx_wait_complete(instance->usart0);

    furi_string_printf(tmp, "Hello from UART1! %lu\r\n", instance->counter);
    furi_hal_serial_tx(instance->uart1, (uint8_t*)furi_string_get_cstr(tmp), furi_string_size(tmp));
    furi_hal_serial_tx_wait_complete(instance->uart1);

    furi_string_free(tmp);

    instance->counter++;
}

static bool demo_service_message_queue_callback(FuriEventLoopObject* object, void* context) {
    DemoService* instance = context;
    furi_check(object == instance->message_queue);

    DemoButton button;
    furi_check(furi_message_queue_get(instance->message_queue, &button, 0) == FuriStatusOk);

    FURI_LOG_I(TAG, "Button pressed: %d", button);

    return true;
}

static DemoService* demo_service_alloc(void) {
    DemoService* instance = malloc(sizeof(DemoService));

    instance->usart0 = furi_hal_serial_control_acquire(FuriHalSerialIdUsart0);
    furi_hal_serial_init(instance->usart0, UART_BAUD_RATE);

    instance->uart1 = furi_hal_serial_control_acquire(FuriHalSerialIdUart1);
    furi_hal_serial_init(instance->uart1, UART_BAUD_RATE);

    instance->event_loop = furi_event_loop_alloc();
    furi_event_loop_tick_set(instance->event_loop, LOG_INTERVAL_MS, demo_service_tick_callback, instance);

    instance->message_queue = furi_message_queue_alloc(16, sizeof(DemoButton));
    furi_event_loop_subscribe_message_queue(instance->event_loop, instance->message_queue, FuriEventLoopEventIn, demo_service_message_queue_callback, instance);

    furi_hal_gpio_init(&gpio_sw_pomodoro, GpioModeInput, GpioPullUp, GpioSpeedHigh);
    furi_hal_gpio_add_int_callback(&gpio_sw_pomodoro, GpioConditionFall, demo_service_pomodoro_int_callback, instance);

    furi_hal_gpio_init(&gpio_sw_busy, GpioModeInput, GpioPullUp, GpioSpeedHigh);
    furi_hal_gpio_add_int_callback(&gpio_sw_busy, GpioConditionFall, demo_service_busy_int_callback, instance);

    furi_hal_gpio_init_simple(&gpio_sw_off, GpioModeInput);
    furi_hal_gpio_add_int_callback(&gpio_sw_off, GpioConditionFall, demo_service_off_int_callback, instance);

    return instance;
}

int32_t demo_srv(void* arg) {
    UNUSED(arg);

    DemoService* instance = demo_service_alloc();
    furi_event_loop_run(instance->event_loop);

    furi_crash();
}
