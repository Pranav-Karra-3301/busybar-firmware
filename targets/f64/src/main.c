#include <furi.h>
#include <furi_hal.h>

#define TAG "Main"

static int32_t init_task(void* context) {
    UNUSED(context);

    furi_hal_init();

    furi_hal_serial_control_set_logging_config(FuriHalSerialIdUlpuart, 230400);

    for(uint32_t counter = 0;; ++counter) {
        FURI_LOG_I("Init", "Hello there! %lu", counter);
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
