#include <furi.h>
#include <furi_hal_gpio.h>

#include <sl_si91x_gpio_common.h>

#define DRIVE_STRENGTH_12MA (3UL)

// static volatile GpioInterrupt gpio_interrupt[GPIO_NUMBER];

void furi_hal_gpio_init_simple(const GpioPin* gpio, const GpioMode mode) {
    furi_hal_gpio_init(gpio, mode, GpioPullNo, GpioSpeedLow);
}

void furi_hal_gpio_init(
    const GpioPin* gpio,
    const GpioMode mode,
    const GpioPull pull,
    const GpioSpeed speed) {
    furi_hal_gpio_init_ex(gpio, mode, pull, speed, GpioAltFnUnused);
}

static void furi_hal_gpio_init_ex_hp_ulp(EGPIO_Type* port, const GpioPin* gpio, const GpioMode mode, const GpioAltFn alt_fn) {
    port->PIN_CONFIG[gpio->pin].GPIO_CONFIG_REG_b.MODE = alt_fn;

    switch(mode) {
    case GpioModeInput:
        port->PIN_CONFIG[gpio->pin].GPIO_CONFIG_REG_b.DIRECTION = SET;
        break;
    case GpioModeOutputPushPull:
        port->PIN_CONFIG[gpio->pin].GPIO_CONFIG_REG_b.DIRECTION = CLR;
        break;
    case GpioModeOutputOpenDrain:
        port->PIN_CONFIG[gpio->pin].GPIO_CONFIG_REG_b.DIRECTION = SET;
        port->PIN_CONFIG[gpio->pin].BIT_LOAD_REG = CLR;
        break;
    default:
        furi_crash();
    }
}

static void furi_hal_gpio_init_uulp(
    const GpioPin* gpio,
    const GpioMode mode,
    const GpioAltFn alt_fn) {
    // Enable Pad receiver (mandatory?)
    UULP_GPIO->NPSS_GPIO_CNTRL[gpio->pin].NPSS_GPIO_CTRLS_b.NPSS_GPIO_REN = SET;
    UULP_GPIO->NPSS_GPIO_CNTRL[gpio->pin].NPSS_GPIO_CTRLS_b.NPSS_GPIO_MODE = alt_fn;

    switch(mode) {
    case GpioModeInput:
        UULP_GPIO->NPSS_GPIO_CNTRL[gpio->pin].NPSS_GPIO_CTRLS_b.NPSS_GPIO_OEN = SET;
        break;
    case GpioModeOutputPushPull:
        UULP_GPIO->NPSS_GPIO_CNTRL[gpio->pin].NPSS_GPIO_CTRLS_b.NPSS_GPIO_OEN = CLR;
        break;
    case GpioModeOutputOpenDrain:
        UULP_GPIO->NPSS_GPIO_CNTRL[gpio->pin].NPSS_GPIO_CTRLS_b.NPSS_GPIO_OEN = SET;
        UULP_GPIO->NPSS_GPIO_CNTRL[gpio->pin].NPSS_GPIO_CTRLS_b.NPSS_GPIO_OUT = CLR;
        break;
    default:
        furi_crash();
    }
}

void furi_hal_gpio_init_ex(
    const GpioPin* gpio,
    const GpioMode mode,
    const GpioPull pull,
    const GpioSpeed speed,
    const GpioAltFn alt_fn) {
    // Configure gpio with interrupts disabled
    FURI_CRITICAL_ENTER();

    switch(gpio->type) {
    case GpioTypeHp:
        // Enable Pad receiver (mandatory?)
        PAD_REG(gpio->pin)->GPIO_PAD_CONFIG_REG_b.PADCONFIG_REN = SET;

        PAD_REG(gpio->pin)->GPIO_PAD_CONFIG_REG_b.PADCONFIG_SR = speed;
        PAD_REG(gpio->pin)->GPIO_PAD_CONFIG_REG_b.PADCONFIG_P1_P2 = pull;
        PAD_REG(gpio->pin)->GPIO_PAD_CONFIG_REG_b.PADCONFIG_E1_E2 = DRIVE_STRENGTH_12MA;

        furi_hal_gpio_init_ex_hp_ulp(GPIO, gpio, mode, alt_fn);
        break;

    case GpioTypeUlp:
        // Enable Pad receiver (mandatory?)
        ULP_PAD_CONFIG2_REG->ULP_PAD_CONFIG_REG2 |= 1UL << gpio->pin;

        // NOTE: Speed and Pull-Up settings are dependent for pins 0...3, 4...8, 8...11
        if(gpio->pin < 4) {
            ULP_PAD_CONFIG0_REG->ULP_GPIO_PAD_CONFIG_REG_0.PADCONFIG_SR_1 = speed;
            ULP_PAD_CONFIG0_REG->ULP_GPIO_PAD_CONFIG_REG_0.PADCONFIG_P1_P2_1 = pull;
            ULP_PAD_CONFIG0_REG->ULP_GPIO_PAD_CONFIG_REG_0.PADCONFIG_E1_E2_1 = DRIVE_STRENGTH_12MA;
        } else if(gpio->pin < 8) {
            ULP_PAD_CONFIG0_REG->ULP_GPIO_PAD_CONFIG_REG_0.PADCONFIG_SR_2 = speed;
            ULP_PAD_CONFIG0_REG->ULP_GPIO_PAD_CONFIG_REG_0.PADCONFIG_P1_P2_2 = pull;
            ULP_PAD_CONFIG0_REG->ULP_GPIO_PAD_CONFIG_REG_0.PADCONFIG_E1_E2_2 = DRIVE_STRENGTH_12MA;
        } else {
            ULP_PAD_CONFIG1_REG->ULP_GPIO_PAD_CONFIG_REG_1.PADCONFIG_SR_1 = speed;
            ULP_PAD_CONFIG1_REG->ULP_GPIO_PAD_CONFIG_REG_1.PADCONFIG_P1_P2_1 = pull;
            ULP_PAD_CONFIG1_REG->ULP_GPIO_PAD_CONFIG_REG_1.PADCONFIG_E1_E2_1 = DRIVE_STRENGTH_12MA;
        }

        furi_hal_gpio_init_ex_hp_ulp(ULP_GPIO, gpio, mode, alt_fn);
        break;

    case GpioTypeUulp:
        furi_hal_gpio_init_uulp(gpio, mode, alt_fn);
        break;

    default:
        furi_crash();
    }

    FURI_CRITICAL_EXIT();
}

void furi_hal_gpio_write(const GpioPin* gpio, const bool state) {
    switch(gpio->type) {
    case GpioTypeHp:
        GPIO->PIN_CONFIG[gpio->pin].BIT_LOAD_REG = state;
        break;
    case GpioTypeUlp:
        ULP_GPIO->PIN_CONFIG[gpio->pin].BIT_LOAD_REG = state;
        break;
    case GpioTypeUulp:
        UULP_GPIO->NPSS_GPIO_CNTRL[gpio->pin].NPSS_GPIO_CTRLS_b.NPSS_GPIO_OUT = state;
        break;
    default:
        furi_crash();
    }
}

void furi_hal_gpio_write_open_drain(const GpioPin* gpio, const bool state) {
    switch(gpio->type) {
    case GpioTypeHp:
        GPIO->PIN_CONFIG[gpio->pin].GPIO_CONFIG_REG_b.DIRECTION = state;
        break;
    case GpioTypeUlp:
        ULP_GPIO->PIN_CONFIG[gpio->pin].GPIO_CONFIG_REG_b.DIRECTION = state;
        break;
    case GpioTypeUulp:
        UULP_GPIO->NPSS_GPIO_CNTRL[gpio->pin].NPSS_GPIO_CTRLS_b.NPSS_GPIO_OEN = state;
        break;
    default:
        furi_crash();
    }
}

bool furi_hal_gpio_read(const GpioPin* gpio) {
    switch(gpio->type) {
    case GpioTypeHp:
        return GPIO->PIN_CONFIG[gpio->pin].BIT_LOAD_REG;
    case GpioTypeUlp:
        return ULP_GPIO->PIN_CONFIG[gpio->pin].BIT_LOAD_REG;
    case GpioTypeUulp:
        return FURI_BIT(UULP_GPIO_STATUS, gpio->pin);
    default:
        furi_crash();
    }
}

// void furi_hal_gpio_add_int_callback(const GpioPin* gpio, GpioExtiCallback cb, void* ctx) {
//     furi_check(gpio);
//     furi_check(cb);
//
//     FURI_CRITICAL_ENTER();
//
//     uint8_t pin_num = furi_hal_gpio_get_pin_num(gpio);
//     furi_check(gpio_interrupt[pin_num].callback == NULL);
//     gpio_interrupt[pin_num].callback = cb;
//     gpio_interrupt[pin_num].context = ctx;
//
//     const uint32_t exti_line = GET_EXTI_LINE(gpio->pin);
//     LL_EXTI_EnableIT_0_31(exti_line);
//
//     FURI_CRITICAL_EXIT();
// }
//
// void furi_hal_gpio_enable_int_callback(const GpioPin* gpio) {
//     furi_check(gpio);
//
//     FURI_CRITICAL_ENTER();
//
//     const uint32_t exti_line = GET_EXTI_LINE(gpio->pin);
//     LL_EXTI_EnableIT_0_31(exti_line);
//
//     FURI_CRITICAL_EXIT();
// }
//
// void furi_hal_gpio_disable_int_callback(const GpioPin* gpio) {
//     furi_check(gpio);
//
//     FURI_CRITICAL_ENTER();
//
//     const uint32_t exti_line = GET_EXTI_LINE(gpio->pin);
//     LL_EXTI_DisableIT_0_31(exti_line);
//     LL_EXTI_ClearFlag_0_31(exti_line);
//
//     FURI_CRITICAL_EXIT();
// }
//
// void furi_hal_gpio_remove_int_callback(const GpioPin* gpio) {
//     furi_check(gpio);
//
//     FURI_CRITICAL_ENTER();
//
//     const uint32_t exti_line = GET_EXTI_LINE(gpio->pin);
//     LL_EXTI_DisableIT_0_31(exti_line);
//     LL_EXTI_ClearFlag_0_31(exti_line);
//
//     uint8_t pin_num = furi_hal_gpio_get_pin_num(gpio);
//     gpio_interrupt[pin_num].callback = NULL;
//     gpio_interrupt[pin_num].context = NULL;
//
//     FURI_CRITICAL_EXIT();
// }

// FURI_ALWAYS_INLINE static void furi_hal_gpio_int_call(uint16_t pin_num) {
//     if(gpio_interrupt[pin_num].callback) {
//         gpio_interrupt[pin_num].callback(gpio_interrupt[pin_num].context);
//     }
// }

/* Interrupt handlers */
// void EXTI0_IRQHandler(void) {
//     if(LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_0)) {
//         furi_hal_gpio_int_call(0);
//         LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_0);
//     }
// }
//
// void EXTI1_IRQHandler(void) {
//     if(LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_1)) {
//         furi_hal_gpio_int_call(1);
//         LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_1);
//     }
// }
//
// void EXTI2_IRQHandler(void) {
//     if(LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_2)) {
//         furi_hal_gpio_int_call(2);
//         LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_2);
//     }
// }
//
// void EXTI3_IRQHandler(void) {
//     if(LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_3)) {
//         furi_hal_gpio_int_call(3);
//         LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_3);
//     }
// }
//
// void EXTI4_IRQHandler(void) {
//     if(LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_4)) {
//         furi_hal_gpio_int_call(4);
//         LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_4);
//     }
// }
//
// void EXTI9_5_IRQHandler(void) {
//     if(LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_5)) {
//         furi_hal_gpio_int_call(5);
//         LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_5);
//     }
//     if(LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_6)) {
//         furi_hal_gpio_int_call(6);
//         LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_6);
//     }
//     if(LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_7)) {
//         furi_hal_gpio_int_call(7);
//         LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_7);
//     }
//     if(LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_8)) {
//         furi_hal_gpio_int_call(8);
//         LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_8);
//     }
//     if(LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_9)) {
//         furi_hal_gpio_int_call(9);
//         LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_9);
//     }
// }
//
// void EXTI15_10_IRQHandler(void) {
//     if(LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_10)) {
//         furi_hal_gpio_int_call(10);
//         LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_10);
//     }
//     if(LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_11)) {
//         furi_hal_gpio_int_call(11);
//         LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_11);
//     }
//     if(LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_12)) {
//         furi_hal_gpio_int_call(12);
//         LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_12);
//     }
//     if(LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_13)) {
//         furi_hal_gpio_int_call(13);
//         LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_13);
//     }
//     if(LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_14)) {
//         furi_hal_gpio_int_call(14);
//         LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_14);
//     }
//     if(LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_15)) {
//         furi_hal_gpio_int_call(15);
//         LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_15);
//     }
// }

// 41: GPIO Wakeup Interrupt
void IRQ041_Handler(void) {
}

// 50: GPIO Group Interrupt0
void IRQ050_Handler(void) {
}

// 51: GPIO Group Interrupt1
void IRQ051_Handler(void) {
}

// 52: GPIO Pin Interrupt0
void IRQ052_Handler(void) {
}

// 53: GPIO Pin Interrupt1
void IRQ053_Handler(void) {
}

// 54: GPIO Pin Interrupt2
void IRQ054_Handler(void) {
}

// 55: GPIO Pin Interrupt3
void IRQ055_Handler(void) {
}

// 56: GPIO Pin Interrupt4
void IRQ056_Handler(void) {
}

// 57: GPIO Pin Interrupt5
void IRQ057_Handler(void) {
}

// 58: GPIO Pin Interrupt6
void IRQ058_Handler(void) {
}

// 59: GPIO Pin Interrupt7
void IRQ059_Handler(void) {
}
