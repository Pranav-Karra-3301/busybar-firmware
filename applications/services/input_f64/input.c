#include <furi.h>
#include "input.h"

#include <furi_hal_serial.h>
#include <furi_hal_resources.h>
#include <furi_hal_serial_control.h>

#define TAG "Input"

#define GPIO_Read(input_pin) (furi_hal_gpio_read(input_pin.pin->gpio) ^ (input_pin.pin->inverted))

#ifdef INPUT_DEBUG
#define INPUT_LOG(...) FURI_LOG_D(TAG, __VA_ARGS__)
#else
#define INPUT_LOG(...)
#endif

typedef struct {
    const InputPin* pin;
    uint16_t debounce_count;
    bool state;
} InputSrvKeyState;

typedef struct {
    FuriEventLoop* event_loop;
    // FuriPubSub* event_pubsub;
    FuriSemaphore* input_semaphore;
    FuriEventLoopTimer* debounce_timer;
    InputSrvKeyState* key_state;
    // InputSrvKeySequence* key_sequence;
    uint32_t sequence_counter;
} InputSrv;

static void input_isr_key(void* context) {
    InputSrv* instance = context;
    furi_semaphore_release(instance->input_semaphore);
}

static void input_debounce_timer_callback(void* context) {
    furi_assert(context);
    InputSrv* instance = context;
    bool is_changing = false;
    for(size_t i = 0; i < input_pins_count; i++) {
        bool state = GPIO_Read(instance->key_state[i]);

        if(state) {
            if(instance->key_state[i].debounce_count < INPUT_DEBOUNCE_TICKS) {
                instance->key_state[i].debounce_count++;
                is_changing = true;
            }
        } else if(instance->key_state[i].debounce_count > 0) {
            instance->key_state[i].debounce_count--;
            is_changing = true;
        }

        if(!is_changing && instance->key_state[i].state != state) {
            instance->key_state[i].state = state;

            FURI_LOG_I(
                TAG, "Key %s %s", instance->key_state[i].pin->name, state ? "pressed" : "released");

            // if(state) {
            //     input_key_sequence_run(
            //         &instance->key_sequence[i], InputTypePress, ++instance->sequence_counter);
            // } else {
            //     input_key_sequence_run(
            //         &instance->key_sequence[i], InputTypeRelease, instance->sequence_counter);
            // }
        }
    }

    if(!is_changing) {
        furi_event_loop_timer_stop(instance->debounce_timer);
    }
}

int32_t input_srv(void* p) {
    UNUSED(p);
    FURI_LOG_I(TAG, "Starting");
    InputSrv* instance = malloc(sizeof(InputSrv));
    instance->input_semaphore = furi_semaphore_alloc(1, 0);
    instance->event_loop = furi_event_loop_alloc();
    instance->debounce_timer = furi_event_loop_timer_alloc(
        instance->event_loop,
        input_debounce_timer_callback,
        FuriEventLoopTimerTypePeriodic,
        instance);

    instance->key_state = malloc(sizeof(InputSrvKeyState) * input_pins_count);
    for(size_t i = 0; i < input_pins_count; i++) {
        furi_hal_gpio_add_int_callback(
            input_pins[i].gpio, input_pins[i].condition, input_isr_key, instance);
        instance->key_state[i].pin = &input_pins[i];
        instance->key_state[i].state = GPIO_Read(instance->key_state[i]);
        instance->sequence_counter = 0;
    }

    // furi_event_loop_subscribe_semaphore(
    //     instance->event_loop,
    //     instance->input_semaphore,
    //     FuriEventLoopEventIn,
    //     input_semaphore_callback,
    //     instance);

    // Start Input Service
    furi_event_loop_run(instance->event_loop);

    return 0;
}
