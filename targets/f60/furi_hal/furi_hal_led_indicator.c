#include <core/log.h>
#include <furi_hal_bus.h>
#include <furi_hal_gpio.h>
#include <furi_hal_resources.h>
#include <stm32u5xx_ll_rcc.h>
#include <stm32u5xx_ll_tim.h>

void furi_hal_led_indicator_set_color(uint8_t red, uint8_t green, uint8_t blue) {
    // TODO: mutex
    // TODO: gamma curve
    LL_TIM_OC_SetCompareCH1(TIM3, red);
    LL_TIM_OC_SetCompareCH1(TIM4, green);
    LL_TIM_OC_SetCompareCH2(TIM4, blue);
}

void furi_hal_led_indicator_init_early(void) {
    furi_hal_bus_enable(FuriHalBusTIM3);
    furi_hal_bus_enable(FuriHalBusTIM4);

    LL_TIM_InitTypeDef tim_cfg = {
        .Prescaler = 160 - 1,
        .CounterMode = LL_TIM_COUNTERMODE_UP,
        .Autoreload = 255 - 1,
        .ClockDivision = LL_TIM_CLOCKDIVISION_DIV1,
        .RepetitionCounter = 0,
    };
    LL_TIM_Init(TIM3, &tim_cfg);
    LL_TIM_Init(TIM4, &tim_cfg);

    LL_TIM_EnableARRPreload(TIM3);
    LL_TIM_EnableARRPreload(TIM4);

    LL_TIM_OC_InitTypeDef tim_oc_cfg = {
        .OCMode = LL_TIM_OCMODE_PWM1,
        .OCState = LL_TIM_OCSTATE_ENABLE,
        .OCNState = LL_TIM_OCSTATE_DISABLE,
        .CompareValue = 0,
        .OCPolarity = LL_TIM_OCPOLARITY_LOW,
        .OCIdleState = LL_TIM_OCIDLESTATE_LOW,
    };

    LL_TIM_OC_Init(TIM3, LL_TIM_CHANNEL_CH1, &tim_oc_cfg);
    LL_TIM_OC_Init(TIM4, LL_TIM_CHANNEL_CH1, &tim_oc_cfg);
    LL_TIM_OC_Init(TIM4, LL_TIM_CHANNEL_CH2, &tim_oc_cfg);

    LL_TIM_OC_EnablePreload(TIM3, LL_TIM_CHANNEL_CH1);
    LL_TIM_OC_EnablePreload(TIM4, LL_TIM_CHANNEL_CH1);
    LL_TIM_OC_EnablePreload(TIM4, LL_TIM_CHANNEL_CH2);

    LL_TIM_EnableCounter(TIM3);
    LL_TIM_EnableCounter(TIM4);

    furi_hal_gpio_init_ex(
        &gpio_top_led_r, GpioModeAltFunctionPushPull, GpioPullNo, GpioSpeedLow, GpioAltFn2TIM3);
    furi_hal_gpio_init_ex(
        &gpio_top_led_g, GpioModeAltFunctionPushPull, GpioPullNo, GpioSpeedLow, GpioAltFn2TIM4);
    furi_hal_gpio_init_ex(
        &gpio_top_led_b, GpioModeAltFunctionPushPull, GpioPullNo, GpioSpeedLow, GpioAltFn2TIM4);
    FURI_LOG_I("Top LED", "Init OK");
}