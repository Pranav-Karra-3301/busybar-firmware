#include <furi.h>
#include <furi_hal.h>

#define TAG "Main"

static int32_t init_task(void* context) {
    UNUSED(context);

    furi_hal_init();
    furi_log_set_level(FuriLogLevelDebug);

    furi_hal_gpio_init_simple(&gpio_10, GpioModeOutputPushPull);
    furi_hal_gpio_init_simple(&gpio_50, GpioModeOutputPushPull);
    furi_hal_gpio_init_simple(&gpio_ulp_2, GpioModeOutputPushPull);
    furi_hal_gpio_init_simple(&gpio_uulp_0, GpioModeOutputPushPull);

    for(;;) {
        furi_hal_gpio_write(&gpio_10, !furi_hal_gpio_read(&gpio_10));
        furi_hal_gpio_write(&gpio_50, !furi_hal_gpio_read(&gpio_50));
        furi_hal_gpio_write(&gpio_ulp_2, !furi_hal_gpio_read(&gpio_ulp_2));
        furi_hal_gpio_write(&gpio_uulp_0, !furi_hal_gpio_read(&gpio_uulp_0));
        furi_delay_ms(500);
    }

    return 0;
}

int main(void) {
    furi_hal_init_early();

    furi_init();

    FuriThread* main_thread = furi_thread_alloc_ex("Init", 4096, init_task, NULL);
    furi_thread_start(main_thread);

    furi_run();

    furi_crash("Kernel is Dead");
}

void abort(void) {
    furi_crash("AbortHandler");
}
