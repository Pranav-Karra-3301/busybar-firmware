/**
 * @file furi_hal_resources.h
 * @brief Hardware resources API
 *
 * @warning GPIO_0...5 are reserved and MUST NOT be used
 */
#pragma once

#include <furi.h>
#include <furi_hal_gpio.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const GpioPin gpio_6;
extern const GpioPin gpio_pwm_red;
extern const GpioPin gpio_ulp_uart_rx;
extern const GpioPin gpio_ulp_uart_tx;
extern const GpioPin gpio_10;
extern const GpioPin gpio_pwm_green;
extern const GpioPin gpio_12;
extern const GpioPin gpio_pwm_blue;
extern const GpioPin gpio_25;
extern const GpioPin gpio_26;
extern const GpioPin gpio_27;
extern const GpioPin gpio_28;
extern const GpioPin gpio_29;
extern const GpioPin gpio_30;
extern const GpioPin gpio_46;
extern const GpioPin gpio_47;
extern const GpioPin gpio_48;
extern const GpioPin gpio_49;
extern const GpioPin gpio_50;
extern const GpioPin gpio_51;
extern const GpioPin gpio_usart0_clk;
extern const GpioPin gpio_53;
extern const GpioPin gpio_usart0_tx;
extern const GpioPin gpio_usart0_rx;
extern const GpioPin gpio_56;
extern const GpioPin gpio_57;

extern const GpioPin gpio_64; /**< Not available on the package, internal use only */
extern const GpioPin gpio_65; /**< Not available on the package, internal use only */
extern const GpioPin gpio_66; /**< Not available on the package, internal use only */
extern const GpioPin gpio_67; /**< Not available on the package, internal use only */
extern const GpioPin gpio_68; /**< Not available on the package, internal use only */
extern const GpioPin gpio_69; /**< Not available on the package, internal use only */
extern const GpioPin gpio_70; /**< Not available on the package, internal use only */
extern const GpioPin gpio_71; /**< Not available on the package, internal use only */
extern const GpioPin gpio_72; /**< Not available on the package, internal use only */
extern const GpioPin gpio_73; /**< Not available on the package, internal use only */
extern const GpioPin gpio_74; /**< Not available on the package, internal use only */
extern const GpioPin gpio_75; /**< Not available on the package, internal use only */

extern const GpioPin gpio_ulp_0;
extern const GpioPin gpio_ulp_1;
extern const GpioPin gpio_ulp_2;
extern const GpioPin gpio_ulp_3; /**< Not available on the package, internal use only */
extern const GpioPin gpio_ulp_4;
extern const GpioPin gpio_ulp_5;
extern const GpioPin gpio_ulp_6;
extern const GpioPin gpio_ulp_7;
extern const GpioPin gpio_uart1_rx;
extern const GpioPin gpio_ulp_9;
extern const GpioPin gpio_ulp_10;
extern const GpioPin gpio_uart1_tx;

extern const GpioPin gpio_uulp_0;
extern const GpioPin gpio_uulp_1;
extern const GpioPin gpio_uulp_2;
extern const GpioPin gpio_uulp_3;

void furi_hal_resources_init_early(void);

void furi_hal_resources_deinit_early(void);

void furi_hal_resources_init(void);

#ifdef __cplusplus
}
#endif
