#include <furi.h>
#include <furi_hal.h>

#define TAG "Main"

static const char* greeting = "Hello there!";
static const char* response = "General Kenobi!";

static int32_t init_task(void* context) {
    UNUSED(context);

    furi_hal_init();

    furi_hal_serial_control_set_logging_config(FuriHalSerialIdUlpuart, 230400);

    FuriHalSerialHandle* usart0 = furi_hal_serial_control_acquire(FuriHalSerialIdUsart0);
    furi_hal_serial_init(usart0, 921600);

    // FuriHalSerialHandle* usart1 = furi_hal_serial_control_acquire(FuriHalSerialIdUart1);
    // furi_hal_serial_init(usart1, 921600);

    FuriString* tmp = furi_string_alloc();

    for(uint32_t counter = 0;; ++counter) {
        FURI_LOG_I(TAG, "%s %lu", greeting, counter);

        furi_string_printf(tmp, "%s %lu \r\n", response, counter);

        furi_hal_serial_tx(usart0, (uint8_t*)furi_string_get_cstr(tmp), furi_string_size(tmp));
        furi_hal_serial_tx_wait_complete(usart0);

        // furi_hal_serial_tx_wait_complete(usart1);
        // furi_hal_serial_tx(usart1, (uint8_t*)furi_string_get_cstr(tmp), furi_string_size(tmp));

        furi_delay_ms(500);
    }

    return 0;
}

int main(void) {
    furi_init();
    furi_log_set_level(FuriLogLevelDebug);

    furi_hal_init_early();

    FuriThread* main_thread = furi_thread_alloc_ex("Init", 4096, init_task, NULL);
    furi_thread_start(main_thread);

    furi_run();

    furi_crash("Kernel is Dead");
}

void abort(void) {
    furi_crash("AbortHandler");
}
