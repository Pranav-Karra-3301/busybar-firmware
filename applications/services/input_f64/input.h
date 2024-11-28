/**
 * @file input.h
 * Input: main API
 */
#pragma once

#include <furi_hal_resources.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Input Types */
typedef enum {
    InputTypePress, /**< Press event, emitted after debounce */
    InputTypeRelease, /**< Release event, emitted after debounce */
    InputTypeTurn, /**< Encoder turn event */
    InputTypeSwitch, /**< Switch position change event */
    InputTypeMAX, /**< Special value for exceptional */
} InputType;

/** Input Event*/
typedef struct {
    InputKey key;
    InputType type;
    union {
        int16_t delta;
        InputSwitchPosition position;
    };
} InputEvent;

#ifdef __cplusplus
}
#endif
