#include <furi.h>
#include <furi_hal_gpio.h>

#include <sl_si91x_gpio_common.h>

#define HP_INT_HANDLER_COUNT (COUNT_OF(GPIO->INTR))

#define DRIVE_STRENGTH_12MA (3UL)
#define GPIO_INTR_STATUS_CLEAR (1UL)

typedef struct {
    const GpioPin* gpio;
    GpioExtiCallback callback;
    void* context;
} GpioInterrupt;

static volatile GpioInterrupt gpio_interrupt[HP_INT_HANDLER_COUNT];

static uint32_t furi_hal_gpio_get_free_interrupt_index(void) {
    for(uint32_t i = 0; i < HP_INT_HANDLER_COUNT; ++i) {
        if(gpio_interrupt[i].callback == NULL) {
            return i;
        }
    }

    furi_crash("Maximum HP interrupt count exceeded");
}

static uint32_t furi_hal_gpio_get_configured_interrupt_index(const GpioPin* gpio) {
    for(uint32_t i = 0; i < HP_INT_HANDLER_COUNT; ++i) {
        if(gpio_interrupt[i].gpio == gpio) {
            return i;
        }
    }

    furi_crash("Gpio not configured as interrupt source");
}

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

    if(mode == GpioModeInput) {
        port->PIN_CONFIG[gpio->pin].GPIO_CONFIG_REG_b.DIRECTION = 1;
    } else if(mode == GpioModeOutputPushPull) {
        port->PIN_CONFIG[gpio->pin].GPIO_CONFIG_REG_b.DIRECTION = 0;
    } else if(mode == GpioModeOutputOpenDrain) {
        port->PIN_CONFIG[gpio->pin].GPIO_CONFIG_REG_b.DIRECTION = 1;
        port->PIN_CONFIG[gpio->pin].BIT_LOAD_REG = 0;
    } else {
        furi_crash();
    }
}

static void furi_hal_gpio_init_uulp(
    const GpioPin* gpio,
    const GpioMode mode,
    const GpioAltFn alt_fn) {
    // Enable Pad receiver (mandatory?)
    UULP_GPIO->NPSS_GPIO_CNTRL[gpio->pin].NPSS_GPIO_CTRLS_b.NPSS_GPIO_REN = 1;
    UULP_GPIO->NPSS_GPIO_CNTRL[gpio->pin].NPSS_GPIO_CTRLS_b.NPSS_GPIO_MODE = alt_fn;

    if(mode == GpioModeInput) {
        UULP_GPIO->NPSS_GPIO_CNTRL[gpio->pin].NPSS_GPIO_CTRLS_b.NPSS_GPIO_OEN = 1;

    } else if(mode == GpioModeOutputPushPull) {
        UULP_GPIO->NPSS_GPIO_CNTRL[gpio->pin].NPSS_GPIO_CTRLS_b.NPSS_GPIO_OEN = 0;

    } else if(mode == GpioModeOutputOpenDrain) {
        UULP_GPIO->NPSS_GPIO_CNTRL[gpio->pin].NPSS_GPIO_CTRLS_b.NPSS_GPIO_OEN = 1;
        UULP_GPIO->NPSS_GPIO_CNTRL[gpio->pin].NPSS_GPIO_CTRLS_b.NPSS_GPIO_OUT = 0;

    } else {
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

    if(gpio->type == GpioTypeHp) {
        // Enable Pad receiver (mandatory?)
        PAD_REG(gpio->pin)->GPIO_PAD_CONFIG_REG_b.PADCONFIG_REN = 1;

        PAD_REG(gpio->pin)->GPIO_PAD_CONFIG_REG_b.PADCONFIG_SR = speed;
        PAD_REG(gpio->pin)->GPIO_PAD_CONFIG_REG_b.PADCONFIG_P1_P2 = pull;
        PAD_REG(gpio->pin)->GPIO_PAD_CONFIG_REG_b.PADCONFIG_E1_E2 = DRIVE_STRENGTH_12MA;

        furi_hal_gpio_init_ex_hp_ulp(GPIO, gpio, mode, alt_fn);

    } else if(gpio->type == GpioTypeUlp) {
        // Enable Pad receiver (mandatory?)
        ULP_PAD_CONFIG2_REG->ULP_PAD_CONFIG_REG2 |= 1UL << gpio->pin;

        // NOTE: Speed and Pull-Up settings are co-dependent for pins 0...3, 4...8, 8...11
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

    } else if(gpio->type == GpioTypeUulp) {
        furi_hal_gpio_init_uulp(gpio, mode, alt_fn);

    } else {
        furi_crash();
    }

    FURI_CRITICAL_EXIT();
}

void furi_hal_gpio_enable_ulp_on_hp(const GpioPin* ulp_gpio, const GpioAltFn alt_fn) {
    furi_check(ulp_gpio);
    furi_check(ulp_gpio->type == GpioTypeUlp);

    ULPCLK->ULP_SOC_GPIO_MODE_REG[ulp_gpio->pin].ULP_SOC_GPIO_MODE_REG_b.ULP_SOC_GPIO_MODE_REG = alt_fn;
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

void furi_hal_gpio_add_int_callback(const GpioPin* gpio, GpioCondition cond, GpioExtiCallback cb, void* ctx) {
    furi_check(gpio);
    furi_check(cb);

    FURI_CRITICAL_ENTER();

    const uint32_t idx = furi_hal_gpio_get_free_interrupt_index();

    gpio_interrupt[idx].gpio = gpio;
    gpio_interrupt[idx].callback = cb;
    gpio_interrupt[idx].context = ctx;

    GPIO->INTR[idx].GPIO_INTR_CTRL_b.PORT_NUMBER = gpio->pin / 16;
    GPIO->INTR[idx].GPIO_INTR_CTRL_b.PIN_NUMBER = gpio->pin % 16;

    if(cond == GpioConditionRise) {
        GPIO->INTR[idx].GPIO_INTR_CTRL_b.RISE_EDGE_ENABLE = 1;
    } else if(cond == GpioConditionFall) {
        GPIO->INTR[idx].GPIO_INTR_CTRL_b.FALL_EDGE_ENABLE = 1;
    } else if(cond == GpioConditionRiseFall) {
        GPIO->INTR[idx].GPIO_INTR_CTRL_b.RISE_EDGE_ENABLE = 1;
        GPIO->INTR[idx].GPIO_INTR_CTRL_b.FALL_EDGE_ENABLE = 1;
    }

    GPIO->INTR[idx].GPIO_INTR_STATUS_b.MASK_CLEAR = 1;

    FURI_CRITICAL_EXIT();
}

void furi_hal_gpio_enable_int_callback(const GpioPin* gpio) {
    furi_check(gpio);

    FURI_CRITICAL_ENTER();

    // TODO: enable callback

    FURI_CRITICAL_EXIT();
}

void furi_hal_gpio_disable_int_callback(const GpioPin* gpio) {
    furi_check(gpio);

    FURI_CRITICAL_ENTER();

    // TODO: disable callback

    FURI_CRITICAL_EXIT();
}

void furi_hal_gpio_remove_int_callback(const GpioPin* gpio) {
    furi_check(gpio);

    FURI_CRITICAL_ENTER();

    // TODO: remove callback

    const uint32_t idx = furi_hal_gpio_get_configured_interrupt_index(gpio);

    gpio_interrupt[idx].gpio = NULL;
    gpio_interrupt[idx].callback = NULL;
    gpio_interrupt[idx].context = NULL;

    FURI_CRITICAL_EXIT();
}

FURI_ALWAYS_INLINE static void furi_hal_gpio_int_call(uint32_t index) {
    if(gpio_interrupt[index].callback) {
        gpio_interrupt[index].callback(gpio_interrupt[index].context);
    }

    GPIO->INTR[index].GPIO_INTR_STATUS = GPIO_INTR_STATUS_CLEAR;
}

/* Interrupt handlers */

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
    furi_hal_gpio_int_call(0);
}

// 53: GPIO Pin Interrupt1
void IRQ053_Handler(void) {
    furi_hal_gpio_int_call(1);
}

// 54: GPIO Pin Interrupt2
void IRQ054_Handler(void) {
    furi_hal_gpio_int_call(2);
}

// 55: GPIO Pin Interrupt3
void IRQ055_Handler(void) {
    furi_hal_gpio_int_call(3);
}

// 56: GPIO Pin Interrupt4
void IRQ056_Handler(void) {
    furi_hal_gpio_int_call(4);
}

// 57: GPIO Pin Interrupt5
void IRQ057_Handler(void) {
    furi_hal_gpio_int_call(5);
}

// 58: GPIO Pin Interrupt6
void IRQ058_Handler(void) {
    furi_hal_gpio_int_call(6);
}

// 59: GPIO Pin Interrupt7
void IRQ059_Handler(void) {
    furi_hal_gpio_int_call(7);
}
