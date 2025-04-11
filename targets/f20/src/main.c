#include <furi.h>
#include <furi_hal.h>
#include <flipper.h>

#include <stm32u5xx_ll_cortex.h>
#include <stm32u5xx_ll_system.h>
#include <stm32u5xx_ll_pwr.h>
#include <stm32u5xx_ll_utils.h>
#include <furi_hal_clock.h>

#define TAG "Main"

int32_t init_task(void* context) {
    UNUSED(context);

    // Flipper FURI HAL
    furi_hal_init();

    // Set the UART for logging output
    furi_hal_serial_control_set_logging_config(FuriHalSerialIdUsart6, 230400);


    furi_log_puts("Flipper Init\r\n");
    furi_delay_ms(5000);
    furi_log_puts("Flipper Init Low Power 1\r\n");
    furi_hal_gpio_init(&gpio_oled_vcc_en, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(&gpio_led_power_en, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_write(&gpio_oled_vcc_en, 0);
    furi_hal_gpio_write(&gpio_led_power_en, 0);

    //furi_hal_clock_suspend_tick();
    LL_PWR_SetPowerMode(PWR_CR1_LPMS_2); //STOP 0
    LL_LPM_EnableDeepSleep();
    __WFI();
    LL_LPM_EnableSleep();

    //furi_hal_clock_resume_tick();
    furi_log_puts("Flipper Init Stop 0\r\n");
    furi_delay_ms(5000);

    // Init flipper
    flipper_init();

    furi_background();

    return 0;
}

int main(void) {
    // Initialize FURI layer
    furi_init();
    furi_log_set_level(FuriLogLevelDebug);

    // Flipper critical FURI HAL
    furi_hal_init_early();

    // for(uint32_t i = 0; i < 1600000; i++) {
    //     __asm volatile("nop");
    // }
    // LL_PWR_SetPowerMode(0); //STOP 0
    // LL_LPM_EnableDeepSleep();
    // __WFI();
    // LL_LPM_EnableSleep();

    FuriThread* main_thread = furi_thread_alloc_ex("Init", 4096, init_task, NULL);

    furi_thread_start(main_thread);

    // Run Kernel
    furi_run();

    furi_crash("Kernel is Dead");
}

void abort(void) {
    furi_crash("AbortHandler");
}
