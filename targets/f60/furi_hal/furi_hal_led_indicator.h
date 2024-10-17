#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void furi_hal_led_indicator_set_color(uint8_t red, uint8_t green, uint8_t blue);

void furi_hal_led_indicator_init_early(void);

#ifdef __cplusplus
}
#endif