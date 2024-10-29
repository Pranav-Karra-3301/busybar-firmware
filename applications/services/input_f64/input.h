/**
 * @file input.h
 * Input: main API
 */

#pragma once
#include <stdint.h>
#include "furi_hal_resources.h"
// #include <furi_hal_resources.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RECORD_INPUT_EVENTS "input_events"
#define INPUT_SEQUENCE_SOURCE_HARDWARE (0u)
#define INPUT_SEQUENCE_SOURCE_SOFTWARE (1u)

typedef enum {
    InputSwitchPositionPomodoro,
    InputSwitchPositionBusy,
    InputSwitchPositionOff,
    InputSwitchPositionApps,
    InputSwitchPositionSettings,
} InputSwitchPosition;

/** Input Types
 * Some of them are physical events and some logical
 */
typedef enum {
    InputTypePress, /**< Press event, emitted after debounce */
    InputTypeRelease, /**< Release event, emitted after debounce */
    InputTypeShort, /**< Short event, emitted after InputTypeRelease done within INPUT_LONG_PRESS interval */
    InputTypeLong, /**< Long event, emitted after INPUT_LONG_PRESS_COUNTS interval, asynchronous to InputTypeRelease  */
    InputTypeRepeat, /**< Repeat event, emitted with INPUT_LONG_PRESS_COUNTS period after InputTypeLong event */
    InputTypeEncoderTurn,
    InputTypeSwitch,
    InputTypeMAX, /**< Special value for exceptional */
} InputType;

/** Input Event, dispatches with FuriPubSub */
typedef struct {
    InputKey key;
    InputType type;
    union {
        int16_t click_count;
        uint16_t repeat_count;
        InputSwitchPosition switch_position;
    };
    //TODO:
    union {
        uint32_t sequence;
        struct {
            uint8_t sequence_source : 2;
            uint32_t sequence_counter : 30;
        };
    };
} InputEvent;

/** Get human readable input key name
 * @param key - InputKey
 * @return string
 */
const char* input_get_key_name(InputKey key);

/** Get human readable input type name
 * @param type - InputType
 * @return string
 */
const char* input_get_type_name(InputType type);

#ifdef __cplusplus
}
#endif
