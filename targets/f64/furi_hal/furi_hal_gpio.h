/**
 * @file furi_hal_gpio.h
 * @brief GPIO HAL library for Si917
 *
 * @note If a pin is configured as open drain output, use furi_hal_gpio_write_open_drain()
 *       instead of furi_hal_gpio_write()
 */
#pragma once

#include <furi.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Interrupt callback prototype
 */
typedef void (*GpioExtiCallback)(void* ctx);

/**
 * Gpio interrupt type
 */
typedef struct {
    GpioExtiCallback callback;
    void* context;
} GpioInterrupt;

/**
 * Gpio modes
 */
typedef enum {
    GpioModeInput,
    GpioModeOutputPushPull,
    GpioModeOutputOpenDrain,
} GpioMode;

/**
 * Gpio pull modes
 */
typedef enum {
    GpioPullNo,
    GpioPullUp,
    GpioPullDown,
} GpioPull;

/**
 * Gpio speed modes
 */
typedef enum {
    GpioSpeedLow,
    GpioSpeedHigh,
} GpioSpeed;

/**
 * Gpio alternate functions
 */
typedef enum {
    GpioAltFnUnused,
} GpioAltFn;

typedef enum {
   GpioTypeHp,
   GpioTypeUlp,
   GpioTypeUulp,
} GpioType;

typedef struct {
    GpioType type;
    uint8_t pin;
} GpioPin;

/**
 * GPIO initialization function, simple version
 * @param gpio  GpioPin
 * @param mode  GpioMode
 */
void furi_hal_gpio_init_simple(const GpioPin* gpio, const GpioMode mode);

/**
 * GPIO initialization function, normal version
 * @param gpio  GpioPin
 * @param mode  GpioMode
 * @param pull  GpioPull
 * @param speed GpioSpeed
 */
void furi_hal_gpio_init(
    const GpioPin* gpio,
    const GpioMode mode,
    const GpioPull pull,
    const GpioSpeed speed);

/**
 * GPIO initialization function, extended version
 * @param gpio  GpioPin
 * @param mode  GpioMode
 * @param pull  GpioPull
 * @param speed GpioSpeed
 * @param alt_fn GpioAltFn
 */
void furi_hal_gpio_init_ex(
    const GpioPin* gpio,
    const GpioMode mode,
    const GpioPull pull,
    const GpioSpeed speed,
    const GpioAltFn alt_fn);

/**
 * Add and enable interrupt
 * @param gpio GpioPin
 * @param cb   GpioExtiCallback
 * @param ctx  context for callback
 */
void furi_hal_gpio_add_int_callback(const GpioPin* gpio, GpioExtiCallback cb, void* ctx);

/**
 * Enable interrupt
 * @param gpio GpioPin
 */
void furi_hal_gpio_enable_int_callback(const GpioPin* gpio);

/**
 * Disable interrupt
 * @param gpio GpioPin
 */
void furi_hal_gpio_disable_int_callback(const GpioPin* gpio);

/**
 * Remove interrupt
 * @param gpio GpioPin
 */
void furi_hal_gpio_remove_int_callback(const GpioPin* gpio);

/**
 * GPIO write pin
 * @param gpio  GpioPin
 * @param state true / false
 */
void furi_hal_gpio_write(const GpioPin* gpio, const bool state);

/**
 * GPIO write pin, open drain mode
 * @param gpio  GpioPin
 * @param state true / false
 */
void furi_hal_gpio_write_open_drain(const GpioPin* gpio, const bool state);

/**
 * GPIO read pin
 * @param gpio GpioPin
 * @return true / false
 */
bool furi_hal_gpio_read(const GpioPin* gpio);

#ifdef __cplusplus
}
#endif
